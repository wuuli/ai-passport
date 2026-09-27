export const STATE_FIELDS = [
  'mode',
  'phase',
  'target_ms',
  'elapsed_ms_0',
  'elapsed_ms_1',
  'error_ms_0',
  'error_ms_1',
  'score_0',
  'score_1',
  'round',
  'starter',
  'current',
  'winner',
  'finished_0',
  'finished_1',
  'automatic_0',
  'automatic_1',
  'target_visible',
  'results_visible',
  'started_us',
  'phase_started_us',
  'last_event_us',
  'last_ok_us',
  'target_random',
  'ai_random',
  'pending_ai_ms',
  'has_ok',
  'exited',
  'raw_target_ms',
  'raw_elapsed_ms_0',
  'raw_elapsed_ms_1'
];

export class FirmwareCore {
  constructor(instance, seed = 1, mode = 0) {
    this.wasm = instance.exports;
    if (typeof this.wasm._initialize === 'function') {
      this.wasm._initialize();
    }
    if (!this.wasm.web_init(seed, mode)) {
      throw new Error('固件时钟初始化失败');
    }
  }

  state() {
    return Object.fromEntries(STATE_FIELDS.map((name, i) => [name, this.wasm.web_state(i)]));
  }

  key(button, nowUs = 0) {
    return this.wasm.web_key(button, nowUs);
  }

  handle(event, nowUs = 0) {
    return this.wasm.web_handle(event, nowUs);
  }

  tick(nowUs = 0) {
    return this.wasm.web_tick(nowUs);
  }

  pause(nowUs = 0) {
    this.wasm.web_pause(nowUs);
  }

  home(nowUs = 0) {
    this.wasm.web_home(nowUs);
  }

  exit() {
    this.wasm.web_exit();
  }

  setFixture(phase, targetMs, elapsed0, elapsed1, score0, score1, starter, current, winner, auto0, auto1, nowUs) {
    return this.wasm.web_set_fixture(
      phase, targetMs, elapsed0, elapsed1,
      score0, score1, starter, current, winner,
      auto0, auto1, nowUs
    );
  }

  targetMs(index) {
    return this.wasm.web_target_ms(index);
  }

  reset(seed = 1, mode = 0) {
    if (!this.wasm.web_init(seed, mode)) {
      throw new Error('无法重新初始化');
    }
  }
}

export const TimeDuelCore = FirmwareCore;

export async function loadFirmware(base = new URL('./', import.meta.url)) {
  async function read(path, json = false) {
    const resolved = new URL(path, base);
    if (typeof fetch === 'function' && resolved.protocol !== 'file:') {
      const response = await fetch(resolved, { cache: 'no-cache' });
      if (!response.ok) throw new Error(`资源加载失败：${path} (${response.status})`);
      return json ? response.json() : response.arrayBuffer();
    }
    const fs = await import('node:fs/promises');
    const url = await import('node:url');
    const fullPath = url.fileURLToPath(resolved);
    const content = await fs.readFile(fullPath);
    if (json) return JSON.parse(content.toString('utf8'));
    return content.buffer.slice(content.byteOffset, content.byteOffset + content.byteLength);
  }

  const [binary, manifest] = await Promise.all([
    read('duel.wasm'),
    read('manifest.json', true),
  ]);

  async function hash(bytes) {
    return Array.from(
      new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)),
      x => x.toString(16).padStart(2, '0')
    ).join('');
  }

  if ((await hash(binary)) !== manifest.artifacts['duel.wasm']) {
    throw new Error('缓存中的代码或素材版本不一致，请重新加载');
  }

  const { instance } = await WebAssembly.instantiate(binary, {});
  const seed = (globalThis.crypto?.getRandomValues?.(new Uint32Array(1))?.[0]) ?? (Date.now() >>> 0);
  return { core: new FirmwareCore(instance, seed, 0), manifest };
}
