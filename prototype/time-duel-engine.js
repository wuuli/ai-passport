const Duel = (() => {
  const targets = Object.freeze(Array.from({ length: 11 }, (_, index) => 1000 + index * 500));
  const create = (mode = 'duo') => ({
    mode, phase: 'home', score: [0, 0], round: 1, starter: 0, current: 0,
    target: null, started: null, phaseStarted: 0, lastOk: -Infinity,
    elapsed: [null, null], automatic: [false, false], winner: null,
  });
  const phase = (state, next, now) => ({ ...state, phase: next, phaseStarted: now });
  const prepare = (state, target, now = 0) => ({
    ...phase(state, 'target', now), target, current: state.starter, started: null,
    elapsed: [null, null], automatic: [false, false], winner: null,
  });
  const home = (state, now) => ({ ...create(state.mode), phaseStarted: now, lastOk: now });
  const stop = (state, elapsed, automatic, now) => {
    const next = { ...state, elapsed: [...state.elapsed], automatic: [...state.automatic] };
    next.elapsed[state.current] = elapsed;
    next.automatic[state.current] = automatic;
    if (next.elapsed.every(value => value !== null)) return phase(next, 'sealed', now);
    return phase({ ...next, current: 1 - state.current }, state.mode === 'ai' ? 'ai' : 'handover', now);
  };
  const settle = (state, now) => {
    const errors = state.elapsed.map(value => Math.abs(value - state.target));
    const winner = errors[0] === errors[1] ? null : errors[0] < errors[1] ? 0 : 1;
    return phase({ ...state, winner, score: state.score.map((score, index) => score + (winner === index ? 1 : 0)) }, 'celebrate', now);
  };
  function reduce(state, action) {
    const now = action.now;
    if (action.type === 'HOME') return home(state, now);
    if (action.type === 'MODE') return state.phase === 'home' ? { ...state, mode: state.mode === 'duo' ? 'ai' : 'duo' } : state;
    if (state.phase === 'timing' && now - state.started >= state.target + 3000) {
      return stop({ ...state, lastOk: action.type === 'OK' ? now : state.lastOk }, state.target + 3000, true, now);
    }
    if (action.type === 'TICK') {
      const age = now - state.phaseStarted;
      if (state.phase === 'ai' && age >= 1100) return stop(state, action.elapsed, false, now);
      if (state.phase === 'celebrate' && age >= 1500) return phase(state, 'result', now);
      if (state.phase === 'final' && age >= 3500) return home(state, now);
      return state;
    }
    if (action.type !== 'OK' || now - state.lastOk < 200 || state.phase === 'ai') return state;
    let next = { ...state, lastOk: now };
    switch (state.phase) {
      case 'home': return prepare(next, action.target, now);
      case 'target':
      case 'handover': return phase({ ...next, started: now }, 'timing', now);
      case 'timing': return stop(next, Math.round(now - state.started), false, now);
      case 'sealed': return settle(next, now);
      case 'celebrate': return phase(next, 'result', now);
      case 'result':
        if (Math.max(...state.score) === 3) return phase(next, 'final', now);
        next.starter = state.mode === 'ai' ? 0 : 1 - state.starter;
        next.round = state.score[0] + state.score[1] + 1;
        return prepare(next, state.winner === null ? state.target : action.target, now);
      case 'final': return home(next, now);
      default: return state;
    }
  }
  function view(state) {
    const visible = ['result', 'final'].includes(state.phase);
    return {
      ...state, started: undefined, phaseStarted: undefined, lastOk: undefined,
      target: ['home', 'timing', 'ai'].includes(state.phase) ? null : state.target,
      winner: visible || state.phase === 'celebrate' ? state.winner : null,
      finished: state.elapsed.map(value => value !== null),
      elapsed: visible ? [...state.elapsed] : [null, null],
      errors: visible ? state.elapsed.map(value => Math.abs(value - state.target)) : [null, null],
    };
  }
  function aiEstimate(target, random = Math.random) {
    const normal = Math.sqrt(-2 * Math.log(Math.max(Number.EPSILON, random()))) * Math.cos(2 * Math.PI * random());
    return Math.max(220, Math.min(target + 2900, Math.round(target * (1 + .12 * normal))));
  }
  return Object.freeze({ targets, create, prepare, reduce, view, aiEstimate });
})();
if (typeof module !== 'undefined') module.exports = Duel;
