import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {FirmwareCore,STATE_FIELDS} from '../prototype/exit-corridor/firmware/runtime.mjs';
import {ThreeKeyInput} from '../prototype/exit-corridor/firmware/input.mjs';
const sprite=fs.readFileSync('assets/images/exit-corridor/commuter-device.bin');
const wasm=fs.readFileSync('prototype/exit-corridor/firmware/corridor.wasm');
const {instance}=await WebAssembly.instantiate(wasm,{});
const core=new FirmwareCore(instance,sprite,1);
assert.equal(core.state().phase,0);assert.equal(core.state().z,Math.fround(-1.8));
let steps=[],snapshots=[];
function command(line){steps.push(line);const [op,...v]=line.split(' ');const n=v.map(Number);
  if(op==='I')core.reset(n[0]);else if(op==='K')core.key(n[0]);else if(op==='T')core.tick(n[0]);
  else if(op==='R')core.review(...n);else if(op==='P')core.pause();else if(op==='H')core.title();
  else if(op==='S')snapshots.push({state:core.state(),image:Uint8Array.from(core.draw())});
}
function tick(n,dt=.05){for(let i=0;i<n;i++)command(`T ${dt}`);}
command('S');command('K 2');command('K 2');tick(430);command('S');
assert.equal(core.state().walking,0);assert.ok(Math.abs(core.state().x-.85)<.001);assert.equal(core.state().passages,0);
command('K 2');tick(140);command('S');assert.equal(core.state().score,1);assert.equal(core.state().cell,1);
for(let a=0;a<=8;a++){
  command(`R ${a} 0 -19.131 ${Math.PI/4} 0 0`);command('S');command('K 1');tick(30);command('S');
  assert.ok(Math.abs(core.state().z+20)<.001);assert.equal(core.state().observing,0);
  for(let entry=0;entry<=1;entry++)for(let side=0;side<=1;side++){
    command(`R ${a} ${side?4.79:-4.79} ${side?-26.8:2.8} ${side?Math.PI/2:-Math.PI/2} 7 ${entry}`);
    command('K 2');command('T 0.1');command('S');
    const correct=(side!==entry)===(a===0);assert.equal(core.state().score,correct?8:0);assert.equal(core.state().phase,correct?2:1);
  }
}
command('I 123');command('K 2');command('K 2');tick(300);command('S');command('P');const p=core.state();tick(10);assert.equal(core.state().z,p.z);command('H');command('S');
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'corridor-web-test-'));
let changed=0,pixels=0,maxColorDelta=0,maxStateError=0;
try{
  const exe=path.join(dir,'native');
  let result=spawnSync(process.env.CC||'cc',['-O2','-ffp-contract=off','-std=c11','-Wall','-Wextra','-Werror','-Imain','tests/corridor_web_native.c','prototype/exit-corridor/firmware/bridge.c','main/corridor_game.c','main/corridor_render.c','-lm','-o',exe],{encoding:'utf8'});
  assert.equal(result.status,0,result.stderr);
  result=spawnSync(exe,[],{input:steps.join('\n')+'\n',maxBuffer:32*1024*1024});assert.equal(result.status,0,result.stderr?.toString());
  const recordSize=25*8+77824;assert.equal(result.stdout.length,recordSize*snapshots.length);
  for(let s=0;s<snapshots.length;s++){
    const base=s*recordSize,expected=snapshots[s];
    for(let f=0;f<25;f++){
      const native=result.stdout.readDoubleLE(base+f*8),web=expected.state[STATE_FIELDS[f]],delta=Math.abs(native-web);
      maxStateError=Math.max(maxStateError,delta);assert.ok(delta<.0002,`${s}/${STATE_FIELDS[f]}: native ${native}, Wasm ${web}`);
    }
    const image=result.stdout.subarray(base+200,base+recordSize);
    assert.deepEqual(image.subarray(0,1024),Buffer.from(expected.image.subarray(0,1024)));
    for(let i=1024;i<77824;i++){
      if(image[i]!==expected.image[i]){changed++;for(let c=0;c<3;c++)maxColorDelta=Math.max(maxColorDelta,Math.abs(image[image[i]*4+c]-expected.image[expected.image[i]*4+c]));}
      pixels++;
    }
  }
  // libm/compiler targets can round a boundary sample differently. Fail on
  // material divergence; exact counts are printed instead of claiming identity.
  assert.ok(changed/pixels<.001,`Pixel mismatch rate ${changed}/${pixels}`);
}finally{fs.rmSync(dir,{recursive:true,force:true});core.destroy();}
// Input timing exercises the actual browser input module, with deterministic timers.
let events=[],timers=new Map(),id=0;
const input=new ThreeKeyInput({key:k=>events.push(k),exit:()=>events.push('exit'),pause:()=>events.push('pause'),delay:cb=>{timers.set(++id,cb);return id;},cancel:i=>timers.delete(i)});
input.press('left',0);assert.deepEqual(events,[0]);assert.equal(input.press('right',1),false);input.release('right');assert.ok(input.active);input.release('left');
input.press('ok',2);assert.deepEqual(events,[0]);input.release('ok');assert.deepEqual(events,[0,2]);assert.equal(timers.size,0);
input.press('held',2);[...timers.values()][0]();input.release('held');assert.deepEqual(events,[0,2,'exit']);
input.press('cancelled',2);input.clear();assert.equal(events.includes(2),true);assert.equal(events.filter(x=>x===2).length,1);assert.equal(timers.size,0);
// Browser timer APIs reject an arbitrary receiver (unlike Node's timers).
// Exercise the default adapter, not just injected test timers.
const savedTimers={set:globalThis.setTimeout,clear:globalThis.clearTimeout};
try {
  let callback,cleared=false;
  globalThis.setTimeout=function(fn,ms){assert.ok(this===undefined||this===globalThis);assert.equal(ms,1000);callback=fn;return 42;};
  globalThis.clearTimeout=function(id){assert.ok(this===undefined||this===globalThis);assert.equal(id,42);cleared=true;};
  const received=[];
  const defaults=new ThreeKeyInput({key:k=>received.push(k),exit:()=>received.push('exit'),pause:()=>{}});
  defaults.press('space',2);callback();defaults.release('space');
  assert.deepEqual(received,['exit']);assert.equal(cleared,true);
} finally {globalThis.setTimeout=savedTimers.set;globalThis.clearTimeout=savedTimers.clear;}
console.log(JSON.stringify({snapshots:snapshots.length,traceCommands:steps.length,maxStateError,pixelSamples:pixels,changedPixels:changed,maxColorDelta,input:'PASS'}));
