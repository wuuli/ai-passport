const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const Duel = require('../prototype/time-duel-engine.js');
const Audio = require('../prototype/time-duel-audio.js');

function sealed(score = [0, 0], elapsed = [1982, 2145], mode = 'duo') {
  return { ...Duel.prepare(Duel.create(mode), 2000), phase: 'sealed', score,
    round: score[0] + score[1] + 1, elapsed, current: 1 };
}
const ok = (state, now, target = 2500) => Duel.reduce(state, { type: 'OK', now, target });
const tick = (state, now, elapsed) => Duel.reduce(state, { type: 'TICK', now, elapsed });

test('review page exposes only same-device two-player play', () => {
  const root = path.resolve(__dirname, '..');
  const html = fs.readFileSync(path.join(root, 'prototype/time-duel-v2.html'), 'utf8');
  const preview = fs.readFileSync(path.join(root, 'prototype/time-duel-preview.js'), 'utf8');
  for (const source of [html, preview]) {
    assert.doesNotMatch(source, /mode-ai|单人练习|AI 教官|AI 练习|AI practice/);
  }
  assert.match(html, /同一台机器，轮流挑战/);
  assert.match(preview, /previewBattery = 86/);
  assert.match(preview, /目标 6 秒，9 秒时自动停止/);
  assert.match(preview, /timeoutScenario \? 6000 : 2000/);
  assert.match(preview, /timeoutScenario \? \[9000, 6145\]/);
  assert.doesNotMatch(preview, /pendingAi|\['OK', 'MODE'\]/);
});

test('targets span 1.0 through 6.0 seconds in 0.5-second steps', () => {
  assert.equal(Duel.targets.length, 11);
  assert.equal(Duel.targets[0], 1000);
  assert.equal(Duel.targets.at(-1), 6000);
  for (let index = 1; index < Duel.targets.length; index++) {
    assert.equal(Duel.targets[index] - Duel.targets[index - 1], 500);
  }
});

test('same-device handover starts with one OK and second stop seals without scoring', () => {
  let state = ok(Duel.create(), 0, 2000);
  state = ok(state, 300);
  assert.equal(state.phase, 'timing');
  state = ok(state, 2282);
  assert.equal(state.phase, 'handover');
  assert.equal(Duel.view(state).target, 2000);
  assert.deepEqual(Duel.view(state).elapsed, [null, null]);
  state = ok(state, 2600);
  assert.equal(state.phase, 'timing');
  state = ok(state, 4745);
  assert.equal(state.phase, 'sealed');
  assert.deepEqual(state.elapsed, [1982, 2145]);
  assert.deepEqual(state.score, [0, 0]);
  assert.equal(state.winner, null);
});

test('sealed waits indefinitely, masks all numbers and retains old score until OK', () => {
  const before = sealed([2, 2]);
  assert.equal(tick(before, 1000000), before);
  const view = Duel.view(before);
  assert.equal(view.winner, null);
  assert.deepEqual(view.score, [2, 2]);
  assert.deepEqual(view.elapsed, [null, null]);
  assert.deepEqual(view.errors, [null, null]);
  const after = ok(before, 100);
  assert.equal(after.phase, 'celebrate');
  assert.equal(after.winner, 0);
  assert.deepEqual(after.score, [3, 2]);
  assert.deepEqual(before.score, [2, 2]);
  assert.deepEqual(Duel.view(after).elapsed, [null, null]);
  assert.deepEqual(Duel.view(after).errors, [null, null]);
  assert.equal(ok(after, 299), after);
});

test('animation lasts 1500 ms, result waits for OK, final returns home at 3500 ms', () => {
  let state = ok(sealed([2, 2]), 100);
  assert.equal(tick(state, 1599), state);
  state = tick(state, 1600);
  assert.equal(state.phase, 'result');
  assert.deepEqual(Duel.view(state).errors, [18, 145]);
  assert.equal(tick(state, 1000000), state);
  state = ok(state, 1000300);
  assert.equal(state.phase, 'final');
  assert.equal(tick(state, 1003799), state);
  state = tick(state, 1003800);
  assert.equal(state.phase, 'home');
  assert.deepEqual(state.score, [0, 0]);
  assert.equal(ok(state, 1003999), state);
  assert.equal(ok(state, 1004000).phase, 'target');
});

test('OK skips animation but never scores twice; final OK returns home not target', () => {
  let state = ok(sealed([2, 0]), 300);
  state = ok(state, 500);
  assert.equal(state.phase, 'result');
  assert.deepEqual(state.score, [3, 0]);
  state = ok(state, 700);
  state = ok(state, 900);
  assert.equal(state.phase, 'home');
});

test('ties retain target and round; duo alternates starters; AI keeps human first', () => {
  for (const mode of ['duo', 'ai']) {
    let state = sealed([1, 1], [1980, 2020], mode);
    for (let round = 0; round < 100; round++) {
      state = ok(state, round * 3000);
      assert.equal(state.winner, null);
      state = tick(state, round * 3000 + 1500);
      state = ok(state, round * 3000 + 2000, 3000);
      assert.equal(state.target, 2000);
      assert.equal(state.round, 3);
      assert.deepEqual(state.score, [1, 1]);
      assert.equal(state.current, mode === 'ai' ? 0 : (round + 1) % 2);
      state = { ...state, phase: 'sealed', elapsed: [1980, 2020] };
    }
  }
});

test('timeout clamps at target plus 3000 ms for both tick and late button', () => {
  for (const target of Duel.targets) {
    for (const event of ['TICK', 'OK']) {
      let state = ok(Duel.create(), 0, target);
      state = ok(state, 200);
      state = Duel.reduce(state, { type: event, now: target + 3200 });
      assert.equal(state.phase, 'handover');
      assert.equal(state.elapsed[0], target + 3000);
      assert.equal(state.automatic[0], true);
    }
  }
});

test('AI independent estimate waits then seals, ignores OK while waiting', () => {
  let state = ok(Duel.create('ai'), 0, 2000);
  state = ok(state, 200);
  state = ok(state, 2200);
  assert.equal(state.phase, 'ai');
  assert.equal(ok(state, 2600), state);
  assert.equal(tick(state, 3299, 2130), state);
  state = tick(state, 3300, 2130);
  assert.equal(state.phase, 'sealed');
  assert.deepEqual(state.score, [0, 0]);
  for (const target of Duel.targets) {
    for (let index = 0; index < 1000; index++) {
      const estimate = Duel.aiEstimate(target);
      assert.ok(estimate >= 220 && estimate <= target + 2900);
    }
  }
});

test('all 32 five-round win paths finish through last-round result and home', () => {
  for (let path = 0; path < 32; path++) {
    let state = Duel.create();
    for (let round = 0; round < 5; round++) {
      state = { ...state, target: 2000, phase: 'sealed', elapsed: (path >> round) & 1 ? [2150, 2000] : [2000, 2150] };
      state = ok(state, round * 3000);
      state = tick(state, round * 3000 + 1500);
      assert.equal(state.phase, 'result');
      state = ok(state, round * 3000 + 2000);
      if (Math.max(...state.score) === 3) {
        assert.equal(state.phase, 'final');
        assert.equal(ok(state, round * 3000 + 2300).phase, 'home');
        break;
      }
      assert.equal(state.phase, 'target');
      assert.ok(round < 4);
    }
  }
});

test('browser six cue PCM hashes match firmware regression baselines', () => {
  const expected = { button: 0xd3596de0, start: 0x65fe8141, stop: 0xb33d176b, win: 0x125254b3, lose: 0x6d30456c, match: 0x58c67a75 };
  for (const [kind, baseline] of Object.entries(expected)) {
    const samples = Audio.samples(kind);
    let hash = 2166136261;
    for (let index = 0; index < 16000; index++) {
      const sample = (samples[index] ?? 0) * 32768;
      hash = Math.imul(hash ^ (sample & 255), 16777619) >>> 0;
      hash = Math.imul(hash ^ ((sample >>> 8) & 255), 16777619) >>> 0;
    }
    assert.equal(hash, baseline, kind);
  }
  const music = Audio.samples('music');
  assert.equal(music.length, 102400);
  assert.ok(music.some(value => value !== 0));
  assert.ok(music.every(value => Math.abs(value) <= 2310 / 32768));
});
