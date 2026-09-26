export const STATE_FIELDS = ['phase','anomaly','x','z','yaw','cameraYaw','walking','turning','observing',
  'score','passages','cell','entryExit','npcX','npcZ','npcHeading','npcDistance','npcTurn','rng',
  'scoreBefore','lastAnomaly','correct','forward','cornerStop','cornerId','hudScore','hudScorePending'];
export class FirmwareCore {
  constructor(instance, sprite, seed = 1) {
    this.wasm = instance.exports;
    this.wasm._initialize();
    if (sprite.byteLength !== this.wasm.web_sprite_size()) throw new Error('人物素材尺寸与固件不一致');
    new Uint8Array(this.wasm.memory.buffer, this.wasm.web_sprite(), sprite.byteLength).set(sprite);
    if (!this.wasm.web_init(seed)) throw new Error('固件渲染器初始化失败');
  }
  state() { return Object.fromEntries(STATE_FIELDS.map((name, i) => [name, this.wasm.web_state(i)])); }
  key(key) { this.wasm.web_key(key); }
  tick(seconds) { this.wasm.web_tick(seconds); }
  pause() { this.wasm.web_pause(); }
  title() { this.wasm.web_title(); }
  reset(seed) { if (!this.wasm.web_init(seed)) throw new Error('无法重新初始化'); }
  review(anomaly, x, z, yaw, score = 0, entry = 0) {
    if (!this.wasm.web_review(anomaly, x, z, yaw, score, entry)) throw new Error('无效的评审位置');
  }
  draw() { return new Uint8Array(this.wasm.memory.buffer, this.wasm.web_draw(), 77824); }
  destroy() { this.wasm.web_destroy(); }
}
export async function loadFirmware(base = new URL('./', import.meta.url)) {
  async function read(path, json = false) {
    const response = await fetch(new URL(path, base), { cache: 'no-cache' });
    if (!response.ok) throw new Error(`资源加载失败：${path} (${response.status})`);
    return json ? response.json() : response.arrayBuffer();
  }
  const [binary, presentationBytes, manifest] = await Promise.all([
    read('corridor.wasm'), read('presentation.json'), read('manifest.json', true)
  ]);
  const sprite = await read(manifest.spriteUrl);
  async function hash(bytes) {
    return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),x=>x.toString(16).padStart(2,'0')).join('');
  }
  if (await hash(binary) !== manifest.artifacts['corridor.wasm'] ||
      await hash(presentationBytes) !== manifest.artifacts['presentation.json'] ||
      await hash(sprite) !== manifest.sources['assets/images/exit-corridor/commuter-device.bin']) {
    throw new Error('缓存中的代码或素材版本不一致，请重新加载');
  }
  const presentation = JSON.parse(new TextDecoder().decode(presentationBytes));
  const { instance } = await WebAssembly.instantiate(binary, {});
  const seed = crypto.getRandomValues(new Uint32Array(1))[0];
  return { core: new FirmwareCore(instance, new Uint8Array(sprite), seed), presentation, manifest };
}
