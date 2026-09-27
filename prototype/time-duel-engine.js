import { loadFirmware } from './time-duel/firmware/runtime.mjs';

export const PHASES = [
  'home',
  'target',
  'timing',
  'handover',
  'ai_wait',
  'sealed',
  'celebrate',
  'result',
  'final'
];

export const targets = Object.freeze(Array.from({ length: 11 }, (_, i) => 1000 + i * 500));

export function buildView(state) {
  const targetVisible = Boolean(state.target_visible);
  const resultsVisible = Boolean(state.results_visible);
  return {
    mode: state.mode === 1 ? 'ai' : 'duo',
    phase: PHASES[state.phase] || 'home',
    phaseIndex: state.phase,
    target: targetVisible ? state.target_ms : null,
    rawTarget: state.raw_target_ms,
    elapsed: resultsVisible ? [state.elapsed_ms_0, state.elapsed_ms_1] : [null, null],
    rawElapsed: [state.raw_elapsed_ms_0, state.raw_elapsed_ms_1],
    errors: resultsVisible ? [state.error_ms_0, state.error_ms_1] : [null, null],
    score: [state.score_0, state.score_1],
    round: state.round,
    starter: state.starter,
    current: state.current,
    winner: state.winner < 0 ? null : state.winner,
    finished: [Boolean(state.finished_0), Boolean(state.finished_1)],
    automatic: [Boolean(state.automatic_0), Boolean(state.automatic_1)],
    targetVisible,
    resultsVisible,
    exited: Boolean(state.exited),
    startedUs: state.started_us,
    phaseStartedUs: state.phase_started_us,
    lastEventUs: state.last_event_us,
    lastOkUs: state.last_ok_us,
    pendingAiMs: state.pending_ai_ms,
  };
}

export async function loadEngine(base) {
  const resolvedBase = base || (typeof window !== 'undefined' && window.location
    ? new URL('./time-duel/firmware/', window.location.href)
    : new URL('./time-duel/firmware/', import.meta.url));
  const { core, manifest } = await loadFirmware(resolvedBase);
  return {
    core,
    manifest,
    targets,
    state() {
      return buildView(core.state());
    },
    rawState() {
      return core.state();
    },
    key(button, nowUs = (typeof performance !== 'undefined' ? performance.now() * 1000 : Date.now() * 1000)) {
      return core.key(button, nowUs);
    },
    handle(event, nowUs = (typeof performance !== 'undefined' ? performance.now() * 1000 : Date.now() * 1000)) {
      return core.handle(event, nowUs);
    },
    tick(nowUs = (typeof performance !== 'undefined' ? performance.now() * 1000 : Date.now() * 1000)) {
      return core.tick(nowUs);
    },
    pause(nowUs = (typeof performance !== 'undefined' ? performance.now() * 1000 : Date.now() * 1000)) {
      core.pause(nowUs);
    },
    home(nowUs = (typeof performance !== 'undefined' ? performance.now() * 1000 : Date.now() * 1000)) {
      core.home(nowUs);
    },
    exit() {
      core.exit();
    },
    reset(seed = (Date.now() >>> 0), mode = 0) {
      core.reset(seed, mode);
    },
    setFixture(phase, targetMs, elapsed0, elapsed1, score0, score1, starter, current, winner, auto0, auto1, nowUs = (typeof performance !== 'undefined' ? performance.now() * 1000 : Date.now() * 1000)) {
      return core.setFixture(phase, targetMs, elapsed0, elapsed1, score0, score1, starter, current, winner, auto0, auto1, nowUs);
    },
    targetMs(index) {
      return core.targetMs(index);
    }
  };
}

export const Duel = {
  PHASES,
  targets,
  buildView,
  view: buildView,
  loadEngine,
};

if (typeof globalThis !== 'undefined') {
  globalThis.Duel = Duel;
}
