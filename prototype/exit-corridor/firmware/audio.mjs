// Browser transport only: the firmware C synthesizer produces all PCM.
// Schedule small buffers ahead; no per-frame oscillator or second sound model.
// The previous maximum (gain 4) now sits at 40%, with headroom up to gain 10.
const MAX_PLAYBACK_GAIN = 10;

export class CorridorAudio {
  constructor(core, {Context = globalThis.AudioContext || globalThis.webkitAudioContext,
    schedule = fn => setInterval(fn, 25), cancel = id => clearInterval(id)} = {}) {
    this.core = core; this.Context = Context; this.schedule = schedule; this.cancel = cancel;
    this.context = null; this.sources = new Set(); this.timer = null;
    this.gain = null; this.volume = 0.4;
    this.enabled = true; this.active = false; this.disposed = false; this.next = 0;
    this.underruns = 0; this.maxGapMs = 0;
  }
  async unlock() {
    if (this.disposed || !this.enabled || !this.Context) return;
    try {
      if (!this.context) {
        this.context = new this.Context();
        // Match desktop listening level without changing shared firmware PCM.
        this.gain = this.context.createGain();
        this.gain.gain.value = this.volume * MAX_PLAYBACK_GAIN;
        this.gain.connect(this.context.destination);
      }
      await this.context.resume();
      if (!this.disposed) this.sync();
    } catch { this.enabled = false; this.sync(); }
  }
  setVolume(volume) {
    if (!Number.isFinite(volume)) return;
    this.volume = Math.min(1, Math.max(0, volume));
    if (this.gain) this.gain.gain.setTargetAtTime(this.volume * MAX_PLAYBACK_GAIN, this.context.currentTime, 0.02);
  }
  buttonLabel() {
    if (!this.Context) return '声音不可用';
    if (!this.enabled) return '声音：关';
    return this.context?.state === 'running' ? '声音：开' : '点击启用声音';
  }
  async activate() {
    // A suspended context needs a gesture, not a toggle to OFF on first click.
    if (this.enabled && this.context?.state === 'running') this.setEnabled(false);
    else { this.setEnabled(true); await this.unlock(); }
  }
  setEnabled(enabled) { this.enabled = enabled; this.sync(); }
  setActive(active) { this.active = active; this.sync(); }
  sync() {
    const play = !this.disposed && this.enabled && this.active && this.context?.state === 'running';
    this.core.audioEnabled(play);
    if (play) {
      if (this.timer === null) this.timer = this.schedule(() => this.pump());
      this.pump();
    } else {
      if (this.timer !== null) this.cancel(this.timer);
      this.timer = null;
      for (const source of this.sources) { source.onended = null; source.stop(); source.disconnect(); }
      this.sources.clear(); this.next = 0;
    }
  }
  pump() {
    if (this.disposed || !this.active || !this.enabled || this.context?.state !== 'running') return;
    if (!this.core.audioRunning()) return;
    const now = this.context.currentTime;
    // Keep adjacent buffers contiguous even with <5 ms remaining. Pushing a
    // still-valid next timestamp forward inserts a small audible gap.
    if (!this.next || this.next < now) {
      if (this.next) { ++this.underruns; this.maxGapMs = Math.max(this.maxGapMs, (now-this.next)*1000); }
      this.next = now + 0.005;
    }
    while (this.next < now + 0.1 && this.core.audioRunning()) {
      const pcm = this.core.audioRender(640);
      const buffer = this.context.createBuffer(1, pcm.length, 16000);
      const data = buffer.getChannelData(0);
      for (let i = 0; i < pcm.length; ++i) data[i] = pcm[i] / 32768;
      const source = this.context.createBufferSource();
      source.buffer = buffer; source.connect(this.gain);
      source.onended = () => { this.sources.delete(source); source.disconnect(); };
      this.sources.add(source); source.start(this.next);
      this.next += pcm.length / 16000;
    }
  }
  dispose() {
    this.disposed = true; this.sync();
    this.context?.close().catch(() => {});
    this.gain?.disconnect();
  }
}
