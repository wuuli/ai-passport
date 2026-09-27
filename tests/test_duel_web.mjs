import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { FirmwareCore, STATE_FIELDS, loadFirmware } from '../prototype/time-duel/firmware/runtime.mjs';

// 1. Target array and loader verification
const loaded = await loadFirmware();
assert.ok(loaded.manifest.artifacts['duel.wasm']);

const wasmBytes = fs.readFileSync('prototype/time-duel/firmware/duel.wasm');
const { instance } = await WebAssembly.instantiate(wasmBytes, {});
const core = new FirmwareCore(instance, 42, 0);

assert.equal(core.targetMs(0), 1000);
assert.equal(core.targetMs(2), 2000);
assert.equal(core.targetMs(10), 6000);

// 2. Build deterministic trace commands
const commands = [];
const snapshots = [];

function cmd(line) {
  commands.push(line);
  const parts = line.trim().split(/\s+/);
  const op = parts[0];
  const args = parts.slice(1).map(Number);
  switch (op) {
    case 'I':
      core.reset(args[0], args[1]);
      break;
    case 'E':
      core.handle(args[0], args[1]);
      break;
    case 'K':
      core.key(args[0], args[1]);
      break;
    case 'T':
      core.tick(args[0]);
      break;
    case 'P':
      core.pause(args[0]);
      break;
    case 'H':
      core.home(args[0]);
      break;
    case 'X':
      core.exit();
      break;
    case 'R':
      core.setFixture(...args);
      break;
    case 'S':
      snapshots.push(core.state());
      break;
    default:
      throw new Error(`Unknown test command: ${line}`);
  }
}

// Scenario 1: Initial state & Home phase
cmd('I 42 0');
cmd('S');
assert.equal(core.state().phase, 0);
assert.equal(core.state().mode, 0);

// Scenario 2: Debounce verification
cmd('K 2 1000000'); // OK at 1.0s -> Target phase (1)
cmd('S');
assert.equal(core.state().phase, 1);

cmd('K 2 1050000'); // OK at 1.05s (< 200ms debounce) -> Ignored
cmd('S');
assert.equal(core.state().phase, 1);

cmd('T 1250000');
cmd('K 2 1250000'); // OK at 1.25s (> 200ms debounce) -> Timing phase (2) for Player 0
cmd('S');
assert.equal(core.state().phase, 2);
assert.equal(core.state().current, 0);

// Scenario 3: Duo mode round execution (Player 0 stops -> Handover -> Player 1 stops -> Sealed -> Reveal -> Result -> Next)
cmd('T 2500000');
cmd('K 2 2500000'); // Player 0 stops at 1250ms -> Handover (3)
cmd('S');
assert.equal(core.state().phase, 3);

cmd('T 2800000');
cmd('K 2 2800000'); // Player 1 starts timing -> Timing (2) for Player 1
cmd('S');
assert.equal(core.state().phase, 2);
assert.equal(core.state().current, 1);

cmd('T 4900000');
cmd('K 2 4900000'); // Player 1 stops at 2100ms -> Sealed (5)
cmd('S');
assert.equal(core.state().phase, 5);
assert.equal(core.state().results_visible, 0);

cmd('T 5200000');
cmd('K 2 5200000'); // Reveal -> Celebrate (6)
cmd('S');
assert.equal(core.state().phase, 6);

// Celebration duration 1.5s
cmd('T 6000000'); // 800ms elapsed (< 1500ms) -> Still Celebrate (6)
cmd('S');
assert.equal(core.state().phase, 6);

cmd('T 6750000'); // 1550ms elapsed (>= 1500ms) -> Result (7)
cmd('S');
assert.equal(core.state().phase, 7);
assert.equal(core.state().results_visible, 1);

cmd('T 7000000');
cmd('K 2 7000000'); // Result OK -> Next round Target (1), round 2
cmd('S');
assert.equal(core.state().phase, 1);
assert.equal(core.state().round, 2);
assert.equal(core.state().starter, 1);

// Scenario 4: Timeout clamp (target_ms + 3000ms, max 9000ms)
cmd('T 7300000');
cmd('K 2 7300000'); // Start timing for Player 1
cmd('S');
assert.equal(core.state().phase, 2);

// Advance time by > 9000ms (max target 6000 + 3000 = 9000ms)
cmd('T 17000000'); // Timeout triggered by tick -> Handover (3) with automatic_1=1
cmd('S');
assert.equal(core.state().phase, 3);
assert.equal(core.state().automatic_1, 1);

// Scenario 5: Return Home & Mode toggle to AI mode
cmd('H 18000000'); // Return home
cmd('S');
assert.equal(core.state().phase, 0);

cmd('K 0 18100000'); // UP button toggles mode to AI (1)
cmd('S');
assert.equal(core.state().mode, 1);

cmd('K 2 18350000'); // Target (1)
cmd('S');
assert.equal(core.state().phase, 1);

cmd('T 18600000');
cmd('K 2 18600000'); // Human starts timing (2)
cmd('S');
assert.equal(core.state().phase, 2);

cmd('T 20700000');
cmd('K 2 20700000'); // Human stops -> AI_WAIT (4)
cmd('S');
assert.equal(core.state().phase, 4);

cmd('K 2 21000000'); // OK is ignored during AI wait
cmd('S');
assert.equal(core.state().phase, 4);

cmd('T 21500000'); // 800ms (< 1100ms AI wait) -> Still AI_WAIT (4)
cmd('S');
assert.equal(core.state().phase, 4);

cmd('T 21900000'); // 1200ms (>= 1100ms AI wait) -> Sealed (5)
cmd('S');
assert.equal(core.state().phase, 5);

cmd('T 22200000');
cmd('K 2 22200000'); // Reveal -> Celebrate (6)
cmd('S');
assert.equal(core.state().phase, 6);

cmd('T 23800000'); // Celebrate elapsed -> Result (7)
cmd('S');
assert.equal(core.state().phase, 7);

// Scenario 6: Tie fixture (equal errors, no winner, retains target on next round)
cmd('R 5 2000 1980 2020 1 1 0 1 -1 0 0 24000000'); // Sealed, errors 20ms & 20ms
cmd('S');
assert.equal(core.state().phase, 5);
assert.equal(core.state().results_visible, 0);
assert.equal(core.state().error_ms_0, 0);
assert.equal(core.state().error_ms_1, 0);

cmd('T 24300000');
cmd('K 2 24300000'); // Reveal -> Celebrate (6), winner -1
cmd('S');
assert.equal(core.state().phase, 6);
assert.equal(core.state().winner, -1);

cmd('T 25900000'); // Advance to Result (7)
cmd('S');
assert.equal(core.state().phase, 7);
assert.equal(core.state().results_visible, 1);
assert.equal(core.state().error_ms_0, 20);
assert.equal(core.state().error_ms_1, 20);

cmd('T 26200000');
cmd('K 2 26200000'); // OK in tie result -> Replays same target 2000ms!
cmd('S');
assert.equal(core.state().phase, 1);
assert.equal(core.state().target_ms, 2000);
assert.deepEqual([core.state().score_0, core.state().score_1], [1, 1]);

// Scenario 7: Match Win & Decider
cmd('R 7 2500 2510 2600 3 2 0 1 0 0 0 27000000'); // Result phase with score 3:2
cmd('S');
assert.equal(core.state().phase, 7);
assert.equal(core.state().score_0, 3);

cmd('T 27300000');
cmd('K 2 27300000'); // OK advances to Match Win (8)
cmd('S');
assert.equal(core.state().phase, 8);

cmd('T 30900000'); // 3600ms elapsed (> 3500ms match win timer) -> Returns Home (0)
cmd('S');
assert.equal(core.state().phase, 0);

// Immediate OK in match win returns home
cmd('R 8 2000 0 0 3 2 0 0 0 0 0 31000000');
cmd('T 31300000');
cmd('K 2 31300000');
cmd('S');
assert.equal(core.state().phase, 0);

// Scenario 8: Return Home via web_home
cmd('K 2 31600000'); // Target (1)
cmd('S');
assert.equal(core.state().phase, 1);

cmd('H 31700000'); // Home
cmd('S');
assert.equal(core.state().phase, 0);

// Scenario 9: Input lost (web_pause cancels current attempt and returns home)
cmd('K 2 32000000'); // Target (1)
cmd('T 32300000');
cmd('K 2 32300000'); // Timing (2)
cmd('S');
assert.equal(core.state().phase, 2);

cmd('P 32500000'); // web_pause -> cancels and returns home (0)
cmd('S');
assert.equal(core.state().phase, 0);

// Scenario 10: Exit and Re-entry
cmd('X'); // Exit
cmd('S');
assert.equal(core.state().exited, 1);

cmd('T 33000000'); // Tick while exited has no effect
cmd('S');
assert.equal(core.state().exited, 1);

cmd('K 2 33100000'); // OK while exited reinitializes and returns home
cmd('S');
assert.equal(core.state().exited, 0);
assert.equal(core.state().phase, 0);

// Scenario 11: DOWN button in Home phase recognized
cmd('K 1 33300000');
cmd('S');
assert.equal(core.state().phase, 0);

// 3. Compile and execute native binary, comparing with Wasm snapshots
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'duel-web-test-'));
let maxStateError = 0;

try {
  const exe = path.join(dir, 'native');
  const compile = spawnSync(
    process.env.CC || 'cc',
    [
      '-O2',
      '-ffp-contract=off',
      '-std=c11',
      '-Wall',
      '-Wextra',
      '-Werror',
      '-Imain',
      'tests/duel_web_trace.c',
      'tools/duel_web_bridge.c',
      'main/duel_clock.c',
      '-lm',
      '-o',
      exe,
    ],
    { encoding: 'utf8' }
  );
  assert.equal(compile.status, 0, compile.stderr);

  const run = spawnSync(exe, [], {
    input: commands.join('\n') + '\n',
    maxBuffer: 16 * 1024 * 1024,
  });
  assert.equal(run.status, 0, run.stderr?.toString());

  const stateBytes = STATE_FIELDS.length * 8; // 31 * 8 = 248 bytes
  assert.equal(run.stdout.length, stateBytes * snapshots.length);

  for (let s = 0; s < snapshots.length; s++) {
    const base = s * stateBytes;
    const expected = snapshots[s];
    for (let f = 0; f < STATE_FIELDS.length; f++) {
      const fieldName = STATE_FIELDS[f];
      const nativeVal = run.stdout.readDoubleLE(base + f * 8);
      const wasmVal = expected[fieldName];
      const delta = Math.abs(nativeVal - wasmVal);
      maxStateError = Math.max(maxStateError, delta);
      assert.ok(
        delta < 0.0001,
        `Snapshot ${s} field ${fieldName}: native ${nativeVal}, wasm ${wasmVal}, delta ${delta}`
      );
    }
  }
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}

console.log(
  JSON.stringify({
    snapshots: snapshots.length,
    traceCommands: commands.length,
    maxStateError,
    parity: 'PASS',
  })
);
