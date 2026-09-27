// Exercise the shipped input adapter with the real Wasm core and a controlled clock.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import vm from 'node:vm';

const root = new URL('../', import.meta.url);
const engineUrl = new URL('prototype/time-duel-engine.js', root);
const source = (await fs.readFile(engineUrl, 'utf8')).replace("'./time-duel/firmware/runtime.mjs'", JSON.stringify(new URL('prototype/time-duel/firmware/runtime.mjs', root).href));
const {loadEngine} = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
const script = (await fs.readFile(new URL('prototype/time-duel-preview.js', root), 'utf8'))
  .replace(/^import .*;\n/, '').replace(/start\(\);\s*$/, 'globalThis.ready = start();');
let now = 1000, nextId = 0, engine, destination;
const tasks = new Map();
const schedule = (fn, delay, interval = 0) => { const id = ++nextId; tasks.set(id, {fn, due:now + delay, interval}); return id; };
const advance = ms => {
  const end = now + ms;
  for (;;) {
    const next = [...tasks].filter(([,task]) => task.due <= end).sort((a,b) => a[1].due - b[1].due)[0];
    if (!next) break;
    const [id, task] = next; now = task.due;
    if (task.interval) task.due += task.interval; else tasks.delete(id);
    task.fn();
  }
  now = end;
};
function target(id = '') {
  const events = new Map();
  return {id, disabled:false, dataset:{}, classList:{add(){},remove(){},toggle(){}},
    innerHTML:'',textContent:'', setAttribute(){}, setPointerCapture(){},
    closest(){return id ? this : null;},
    addEventListener(name, fn){events.set(name, [...(events.get(name)||[]),fn]);},
    emit(name, data = {}){for(const fn of events.get(name)||[]) fn({target:this,preventDefault(){},...data});}
  };
}
const ids = ['screen','seat-0','seat-1','up','down','ok','sound','reset','game-round','state-index','state-title','state-copy','state-facts','demo-label','audio-note'];
const elements = new Map(ids.map(id => [id, target(id)]));
const window = target(); window.location = {assign(url){destination=url;}};
const document = target(); document.hidden = false; document.getElementById = id => {
  assert(elements.has(id), `Unknown DOM id ${id}`); return elements.get(id);
};
const context = vm.createContext({window,document,console,performance:{now:()=>now},
  loadEngine: async () => engine = await loadEngine(new URL('prototype/time-duel/firmware/',root)),
  DuelAudio:{create:()=>({available:true,unlocked:true,update(){},stop(){},unlock(){}})},
  setTimeout:(fn,delay)=>schedule(fn,delay),clearTimeout:id=>tasks.delete(id),
  setInterval:(fn,delay)=>schedule(fn,delay,delay),clearInterval:id=>tasks.delete(id)});
vm.runInContext(script,context); await context.ready;
assert(engine, 'Wasm must load');
assert.equal(elements.get('ok').disabled,false);
const down = code => window.emit('keydown',{code,target:elements.get('ok'),repeat:false});
const up = code => window.emit('keyup',{code,target:elements.get('ok')});
const tap = code => {down(code);up(code);};
assert.equal(engine.state().phase,'home');
tap('ArrowUp'); assert.equal(engine.state().mode,'ai');
tap('ArrowUp'); assert.equal(engine.state().mode,'duo');
tap('Space'); assert.equal(engine.state().phase,'target');
advance(250); down('Space'); assert.equal(engine.state().phase,'timing');
window.emit('keydown',{code:'Space',repeat:true});
advance(300); assert.equal(engine.state().phase,'timing');
up('Space'); assert.equal(engine.state().phase,'timing','release must not stop');
advance(200); tap('Space'); assert.equal(engine.state().phase,'handover');
assert.equal(engine.rawState().raw_elapsed_ms_0,500);
assert.deepEqual(engine.state().elapsed,[null,null]);
advance(250); tap('Space'); assert.equal(engine.state().phase,'timing');
advance(700); tap('Space'); assert.equal(engine.state().phase,'sealed');
assert.deepEqual(engine.state().elapsed,[null,null]);
assert.deepEqual(engine.state().score,[0,0]);
advance(250);tap('Space');assert.equal(engine.state().phase,'celebrate');
advance(1516);assert.equal(engine.state().phase,'result');
assert.deepEqual(engine.state().elapsed,[500,700]);
elements.get('reset').onclick(); assert.equal(engine.state().phase,'home');
advance(250);tap('Space');advance(250);tap('Space');
window.emit('blur'); assert.equal(engine.state().phase,'home');
advance(5000);assert.equal(engine.state().phase,'home');
window.emit('focus'); advance(250);tap('Space');advance(250);tap('Space');
elements.get('ok').emit('pointercancel');assert.equal(engine.state().phase,'home');
advance(250);tap('Space');advance(250);tap('Space');
document.hidden=true;document.emit('visibilitychange');assert.equal(engine.state().phase,'home');
document.hidden=false;document.emit('visibilitychange');
advance(250);down('Space');advance(1000);
assert.equal(destination,'/games.html?selected=1');
assert.equal(engine.state().exited,true);
up('Space');assert.equal(engine.state().exited,true);
assert.equal(tasks.size,0,'exit must clear tick and hold timers');
console.log('Time preview input: PASS (real Wasm, PRESS/release/repeat, duo results, mode, blur/cancel/hidden, long exit)');
