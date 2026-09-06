const DuelAudio = (() => {
  const rate = 16000;
  const melody = [440, 0, 659, 440, 784, 659, 523, 0, 587, 0, 698, 587, 880, 784, 659, 0, 523, 659, 784, 0, 698, 587, 440, 0, 494, 587, 659, 784, 659, 494, 440, 0];
  const bass = [110, 147, 131, 165];
  const cues = {
    button: [[1047, 28]], start: [[784, 24]], stop: [[523, 40]],
    win: [[659, 80], [784, 80], [988, 120]], lose: [[392, 100], [330, 100], [262, 120]],
    match: [[523, 90], [659, 90], [784, 90], [0, 60], [784, 90], [1047, 220]],
  };
  function voice(state, frequency, position, duration, amplitude) {
    if (!frequency || position >= duration) return 0;
    state.phase = (state.phase + frequency) % rate;
    const triangle = state.phase < 8000 ? state.phase * 2 - 8000 : 24000 - state.phase * 2;
    return Math.trunc(Math.trunc(triangle * amplitude / 8000) * Math.min(64, position, duration - position) / 64);
  }
  function samples(kind) {
    if (kind === 'music') {
      const lead = { phase: 0 }, low = { phase: 0 };
      return Float32Array.from({ length: 102400 }, (_, index) => {
        const step = Math.floor(index / 3200), position = index % 3200;
        return (voice(lead, melody[step], position, 2700, 1540) + voice(low, bass[Math.floor(step / 8)], position, 1600, 770)) / 32768;
      });
    }
    return Float32Array.from(cues[kind].flatMap(([frequency, duration]) => {
      const oscillator = { phase: 0 }, length = duration * 16;
      return Array.from({ length }, (_, position) => voice(oscillator, frequency, position, length, 2600) / 32768);
    }));
  }
  function create() {
    let context, music, cue, available = true;
    const buffers = new Map();
    function unlock() {
      const Engine = window.AudioContext || window.webkitAudioContext;
      if (!Engine) { available = false; return; }
      try {
        context ||= new Engine();
        context.resume().catch(() => { available = false; });
      } catch { available = false; }
    }
    function play(kind, loop = false) {
      if (!buffers.has(kind)) {
        const data = samples(kind), buffer = context.createBuffer(1, data.length, rate);
        buffer.copyToChannel(data, 0);
        buffers.set(kind, buffer);
      }
      const source = context.createBufferSource();
      source.buffer = buffers.get(kind);
      source.loop = loop;
      source.connect(context.destination);
      source.start();
      return source;
    }
    function stop() {
      music?.stop(); cue?.stop(); music = cue = undefined;
    }
    function update(enabled, musicEnabled, kind) {
      if (!enabled || !context || !available) { stop(); return; }
      if (!musicEnabled) { music?.stop(); music = undefined; }
      else if (!music) music = play('music', true);
      cue?.stop(); cue = undefined;
      if (kind) cue = play(kind);
    }
    return { unlock, update, stop, get available() { return available; }, get unlocked() { return context?.state === 'running'; } };
  }
  return { create, samples };
})();
if (typeof module !== 'undefined') module.exports = DuelAudio;
