// Export real C/Wasm reference workloads, then apply device-load experiments.
// The generic resource model does not implement another game or sound engine.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {FirmwareCore} from '../prototype/exit-corridor/firmware/runtime.mjs';

const outputArg = process.argv.indexOf('--output-dir');
const output = outputArg < 0 ? fs.mkdtempSync(path.join(os.tmpdir(), 'corridor-resource-stress-')) : process.argv[outputArg + 1];
assert.ok(output, '--output-dir requires a directory');
fs.mkdirSync(output, {recursive: true});
const bytes = fs.readFileSync('prototype/exit-corridor/firmware/corridor.wasm');
const sprite = fs.readFileSync('assets/images/exit-corridor/commuter-device.bin');
const module = await WebAssembly.compile(bytes);
const hash = data => createHash('sha256').update(data).digest('hex');
const profile = JSON.parse(fs.readFileSync('tests/fixtures/corridor_resource_profile.json', 'utf8'));
const scenarios = {
  lifecycle: {seconds: 12, setup() {}, actions: new Map([
    [2, c => c.key(2)], [3, c => c.key(2)], [5, c => c.pause()],
    [6, c => c.key(1)], [10, c => c.title()], [11, c => c.key(2)],
  ])},
  corner: {seconds: 6, setup(c) { c.review(0, 0, -25.1, 0); c.key(2); }, actions: new Map()},
  completion: {seconds: 5, setup(c) { c.review(0, 0, -10.9, 0, 8); c.key(2); }, actions: new Map()},
};

function exportTrace(name, scenario) {
  const core = new FirmwareCore(new WebAssembly.Instance(module, {}), sprite, 12345);
  core.audioEnabled(true);
  scenario.setup(core);
  const trace = {schema: 2, duration_us: scenario.seconds * 1e6,
    source: `Actual C/Wasm ${name} reference workload; 20 ms tick, seed 12345`,
    wasm_sha256: hash(bytes), manifest_sha256: hash(fs.readFileSync('prototype/exit-corridor/firmware/manifest.json')),
    events: []};
  const states = [];
  let previous = '';
  for (let step = 0; step < scenario.seconds * 50; ++step) {
    scenario.actions.get(step / 50)?.(core);
    const state = core.state();
    const flags = {audio: core.audioRunning(), render: state.phase === 1 || state.phase === 3,
      phase: state.phase, walking: Boolean(state.walking), turning: Boolean(state.turning)};
    if (JSON.stringify(flags) !== previous) {
      trace.events.push({at_us: step * 20000, name, ...flags,
        active: [...(flags.render ? ['display'] : []), ...(flags.audio ? ['playback'] : [])]});
      previous = JSON.stringify(flags);
    }
    // Execute the actual source as well as extracting its workload gates.
    core.tick(0.02);
    const pcm = core.audioRender(320);
    if (state.phase === 0) assert.ok(pcm.every(s => s === 0), `${name}: title must be silent`);
    states.push(core.state());
  }
  if (name === 'lifecycle') {
    assert.ok(trace.events.some(e => !e.walking && e.audio && e.at_us >= 5e6 && e.at_us < 10e6),
      'Stopping movement must preserve the actual ambience gate');
    assert.ok(trace.events.some(e => e.at_us === 10e6 && !e.audio && !e.render));
    assert.ok(trace.events.some(e => e.at_us === 11e6 && e.audio && e.render));
  } else if (name === 'corner') {
    assert.ok(states.some(s => s.turning), 'fixture must execute an assisted arc');
    assert.ok(states.at(-1).cornerStop, 'fixture must reach the automatic corner stop');
  } else {
    assert.equal(states.at(-1).phase, 2, 'fixture must reach completion');
    assert.equal(core.audioRunning(), false);
  }
  core.destroy();
  return trace;
}

function run(name, candidate, tracePath, expectedExit = 0, requireCalibration = false) {
  const profilePath = path.join(output, `${name}-profile.json`);
  const reportPath = path.join(output, `${name}-report.json`);
  fs.writeFileSync(profilePath, JSON.stringify(candidate, null, 2) + '\n');
  const args = ['tools/resource_stress.py', '--profile', profilePath, '--trace', tracePath, '--output', reportPath];
  if (requireCalibration) args.push('--require-calibrated');
  const process = spawnSync('python3', args, {encoding: 'utf8'});
  assert.equal(process.status, expectedExit, `${name}: ${process.stderr || process.stdout}`);
  return JSON.parse(fs.readFileSync(reportPath, 'utf8'));
}

const results = {};
try {
  for (const [name, scenario] of Object.entries(scenarios)) {
    const trace = exportTrace(name, scenario);
    const tracePath = path.join(output, `${name}-wasm-trace.json`);
    fs.writeFileSync(tracePath, JSON.stringify(trace, null, 2) + '\n');
    const positive = run(name, profile, tracePath);
    assert.equal(positive.result, 'PASS');
    assert.equal(positive.tasks.playback.underrun_us, 0);
    assert.equal(positive.hardware_acceptance, 'NOT RUN');
    const old = structuredClone(profile);
    old.tasks[0].wait = 'busy'; old.tasks[1].priority = 3;
    const regression = run(`${name}-old-busy-wait`, old, tracePath, 1);
    assert.ok(regression.tasks.playback.underrun_us > 0 ||
      regression.tasks.playback.max_startup_us > old.tasks[1].limits.max_startup_us,
      'old scheduling must fail for audio starvation or delayed first write');
    const stall = structuredClone(profile);
    stall.stalls = [{at_us: name === 'lifecycle' ? 4e6 : 1e6, duration_us: 120000}];
    const delayed = run(`${name}-cpu-stall`, stall, tracePath, 1);
    assert.ok(delayed.tasks.playback.underrun_us > 0, 'nonpreemptible delay must exhaust PCM');
    const memory = structuredClone(profile);
    memory.memory.allocations = [{name: 'oversized additional framebuffer', bytes: 153600}];
    const exhausted = run(`${name}-memory-overrun`, memory, tracePath, 1);
    assert.ok(exhausted.failures.some(x => x.includes('largest block')));
    const calibration = run(`${name}-require-calibration`, profile, tracePath, 1, true);
    assert.ok(calibration.failures.some(x => x.includes('calibration is incomplete')));
    results[name] = {positive: positive.result, oldBusyWaitDetected: true, cpuStallDetected: true,
      memoryOverrunDetected: true, incompleteCalibrationBlocked: true, metrics: positive.metrics, tasks: positive.tasks};
  }
  fs.writeFileSync(path.join(output, 'summary.json'), JSON.stringify({modelRegression: 'PASS',
    wasmWorkloadIntegration: 'PASS', calibratedResourceAcceptance: 'NOT RUN',
    hardwareAcceptance: 'NOT RUN', results}, null, 2) + '\n');
  console.log('Corridor C/Wasm resource stress: 3 reference flows and 12 negative controls PASS; calibration PARTIAL');
  if (outputArg >= 0) console.log(`Reports: ${output}`);
} finally {
  if (outputArg < 0) fs.rmSync(output, {recursive: true, force: true});
}
