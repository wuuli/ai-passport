// Offline audition of actual C game events and PCM. No alternate JS sound model.
export const AUDIO_REVIEWS = {
  ambience: {label: '静止环境声', seconds: 6},
  walking: {label: '行走 → 停步', seconds: 10},
  corner: {label: '拐角自动转向 → 停步', seconds: 6},
  observing: {label: '原地观察（应无脚步）', seconds: 5},
  completion: {label: '走出出口 → 通关', seconds: 5},
  lifecycle: {label: '开始 → 行走 → 停步 → 返回开始界面', seconds: 15},
};
export const REVIEW_GAIN = 6; // Accepted desktop reference: slider 60% × gain 10.

export function renderAudioReview(core, kind) {
  const spec = AUDIO_REVIEWS[kind];
  if (!spec) throw new RangeError('Unknown audio review');
  core.audioEnabled(false);
  if (kind === 'lifecycle') core.reset(12345);
  else if (kind === 'corner') core.review(0, 0, -25.1, 0);
  else if (kind === 'completion') core.review(0, 0, -10.9, 0, 8);
  else core.review(0, 0, -2, 0);
  core.audioEnabled(true);
  if (['walking', 'corner', 'completion'].includes(kind)) core.key(2);
  const samples = new Int16Array(spec.seconds * 16000);
  const events = [];
  let peak = 0, squares = 0, clipped = 0;
  for (let block = 0; block < samples.length / 320; ++block) {
    if (kind === 'lifecycle') {
      if (block === 100 || block === 150) core.key(2); // enter at 2 s, walk at 3 s
      if (block === 350) core.pause(); // stop at 7 s
      if (block === 500) core.title(); // return to title at 10 s
    }
    if (kind === 'walking' && block === 300) core.pause();
    if (kind === 'observing' && (block === 25 || block === 100)) core.key(1);
    core.tick(0.02);
    const raw = core.audioRender(320);
    for (let i = 0; i < raw.length; ++i) {
      const amplified = raw[i] * REVIEW_GAIN;
      if (amplified > 32767 || amplified < -32768) ++clipped;
      const sample = Math.min(32767, Math.max(-32768, amplified));
      samples[block * 320 + i] = sample;
      peak = Math.max(peak, Math.abs(sample)); squares += sample * sample;
    }
    if (block % 25 === 0) events.push({seconds: block / 50, ...core.state()});
  }
  return {samples, events, peak: peak / 32768, rms: Math.sqrt(squares / samples.length) / 32768, clipped};
}

export function encodeWav(samples) {
  const bytes = new Uint8Array(44 + samples.length * 2), view = new DataView(bytes.buffer);
  const text = (offset, value) => [...value].forEach((char, i) => view.setUint8(offset + i, char.charCodeAt(0)));
  text(0, 'RIFF'); view.setUint32(4, bytes.length - 8, true); text(8, 'WAVE'); text(12, 'fmt ');
  view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true);
  view.setUint32(24, 16000, true); view.setUint32(28, 32000, true);
  view.setUint16(32, 2, true); view.setUint16(34, 16, true); text(36, 'data');
  view.setUint32(40, samples.length * 2, true);
  samples.forEach((sample, i) => view.setInt16(44 + i * 2, sample, true));
  return bytes;
}
