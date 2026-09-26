import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {FirmwareCore,STATE_FIELDS} from '../prototype/exit-corridor/firmware/runtime.mjs';
import {ThreeKeyInput} from '../prototype/exit-corridor/firmware/input.mjs';
import {FirmwareDisplay} from '../prototype/exit-corridor/firmware/display.mjs';
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
command('K 2');tick(140);command('S');assert.equal(core.state().score,1);assert.equal(core.state().hudScore,1);assert.equal(core.state().cell,1);
// Manual takeover at different arc fractions must retain the eight-way grid
// in both the native build and the actual browser module, without a camera snap.
const cornerStarts=[[0,-25.1,0],[0,1.1,Math.PI],[-1.7,2.8,Math.PI/2],[1.7,-26.8,-Math.PI/2]];
for(const [x,z,yaw] of cornerStarts)for(const fraction of [.2,.3,.7])for(const key of [0,1]){
  command(`R 0 ${x} ${z} ${yaw} 0 0`);command('K 2');
  for(let i=0;i<100&&!core.state().turning;i++)tick(1,.025);
  assert.equal(core.state().turning,1);tick(Math.floor(1.15*fraction/.025),.025);
  command('K 2');const paused=core.state();tick(10);command('S');
  assert.equal(core.state().x,paused.x);assert.equal(core.state().z,paused.z);
  assert.equal(core.state().yaw,paused.yaw);
  const before=core.state();command(`K ${key}`);const after=core.state();
  assert.equal(after.turning,0);assert.equal(after.walking,0);
  assert.equal(after.x,before.x);assert.equal(after.z,before.z);assert.equal(after.cameraYaw,before.cameraYaw);
  const grid=after.yaw/(Math.PI/4);assert.ok(Math.abs(grid-Math.round(grid))<.00001);
  assert.equal(after.passages,0);tick(20);command('S');
  for(let i=0;i<8&&Math.cos(core.state().yaw-yaw)<.99999;i++)command('K 1');
  assert.ok(Math.cos(core.state().yaw-yaw)>.99999);
  command('K 2');tick(40);command('S');
  assert.equal(core.state().cornerStop,1);assert.equal(core.state().walking,0);
  assert.equal(core.state().passages,0);
}
for(let a=0;a<=8;a++){
  command(`R ${a} 0 -19.131 ${Math.PI/4} 0 0`);command('S');command('K 1');tick(30);command('S');
  assert.ok(Math.abs(core.state().z+20)<.001);assert.equal(core.state().observing,0);
  for(let entry=0;entry<=1;entry++)for(let side=0;side<=1;side++){
    command(`R ${a} ${side?4.79:-4.79} ${side?-26.8:2.8} ${side?Math.PI/2:-Math.PI/2} 7 ${entry}`);
    command('K 2');command('T 0.1');command('S');
    const correct=(side!==entry)===(a===0);assert.equal(core.state().score,correct?8:0);assert.equal(core.state().phase,correct?3:1);
    assert.equal(core.state().hudScore,7);assert.equal(core.state().hudScorePending,1);
  }
}
// Both winning portal directions lead to a playable exit, not an instant replay.
for(let side=0;side<=1;side++){
  command(`R ${side?0:5} ${side?4.79:-4.79} ${side?-26.8:2.8} ${side?Math.PI/2:-Math.PI/2} 7 0`);
  command('K 2');tick(1);command('S');assert.equal(core.state().phase,3);
  assert.equal(core.state().hudScore,7);
  tick(130);command('S');assert.equal(core.state().phase,3);assert.equal(core.state().walking,0);
  assert.equal(core.state().hudScore,8);
  command('K 2');tick(65);command('S');
  command('K 2');const paused=core.state();tick(10);command('S');
  assert.equal(core.state().z,paused.z);assert.equal(core.state().score,8);
  command('K 1');tick(8);command('S');command('K 0');tick(8);
  command('K 2');tick(45);command('S');tick(120);command('S');
  assert.equal(core.state().phase,2);assert.equal(core.state().score,8);assert.equal(core.state().passages,1);
  tick(20);assert.equal(core.state().phase,2);
  command('K 0');command('K 1');assert.equal(core.state().phase,2);
  command('K 2');command('S');assert.equal(core.state().phase,1);assert.equal(core.state().score,0);assert.equal(core.state().walking,0);
}
command('I 123');command('K 2');command('K 2');tick(300);command('S');command('P');const p=core.state();tick(10);assert.equal(core.state().z,p.z);command('H');command('S');
// Exercise the real Canvas composition shell: EXITING keeps the world exposed.
for(let entry=0;entry<2;entry++)for(let turn=0;turn<8;turn++){
  command(`R 0 0 ${entry?-17.2:-6.8} ${turn*Math.PI/4} 8 ${entry}`);command('S');
  assert.equal(core.state().phase,3);assert.equal(core.state().score,8);
}
assert.throws(()=>core.review(0,0,-11,0,8,0),/无效/);
assert.throws(()=>core.review(0,0,-13,0,8,1),/无效/);
assert.throws(()=>core.review(0,0,-7,0,9,0),/无效/);
const labels={},canvas={getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}}),setAttribute:(k,v)=>{labels[k]=v;}};
const display=new FirmwareDisplay(canvas,JSON.parse(fs.readFileSync('prototype/exit-corridor/firmware/presentation.json','utf8')));
const presentation=JSON.parse(fs.readFileSync('prototype/exit-corridor/firmware/presentation.json','utf8'));
assert.equal(presentation.ui.title,'8号出口');
assert.equal(presentation.ui.subtitle,'地下通道');
assert.equal(presentation.ui.start,'按 OK 进入');
assert.equal(presentation.ui.guide,'顶部键：左转\n中部键：右转\n底部 OK 键：行走／停步\n拐角自动转向，停后按 OK');
assert.equal(presentation.ui.replay,'按 OK 再走一次');
assert.equal(Object.keys(presentation.titleFont.mapping).length,4);
assert.ok(display.width(presentation.ui.title,presentation.titleFont)>100);
assert.ok(display.width(presentation.ui.title,presentation.titleFont)<200);
for(const char of presentation.ui.title)assert.ok(presentation.titleFont.mapping[char.codePointAt(0)]);
const scene=Uint8Array.from(core.draw()),sceneState={...core.state(),phase:1,score:8,hudScore:8};
display.render(scene,sceneState);const playingPixels=Uint8ClampedArray.from(display.frame.data);
display.render(scene,{...sceneState,phase:0});assert.match(labels['aria-label'],/8号出口，.*拐角自动转向，停后按 OK.*按 OK 进入/);
display.render(scene,{...sceneState,phase:3});assert.deepEqual(display.frame.data,playingPixels);assert.match(labels['aria-label'],/出口 8/);
display.render(scene,{...sceneState,phase:2});assert.notDeepEqual(display.frame.data,playingPixels);assert.match(labels['aria-label'],/已走出通道/);
// Endpoint prompts settle instead of pulsing indefinitely.
display.render(scene,{...sceneState,phase:0},false,1000);
const openingPixels=Uint8ClampedArray.from(display.frame.data);
display.render(scene,{...sceneState,phase:2},false,1000);
assert.ok(display.frame.data[40*240*4]>openingPixels[40*240*4]+100);
display.render(scene,{...sceneState,phase:2},false,1800);
const settledPixels=Uint8ClampedArray.from(display.frame.data);
display.render(scene,{...sceneState,phase:2},false,8000);
assert.deepEqual(display.frame.data,settledPixels);
display.render(scene,sceneState);assert.deepEqual(display.frame.data,playingPixels);
// Detailed layout coordinates, bounding boxes, font subset coverage, and 800ms fade curve
const titleChars = [..."8号出口"];
assert.equal(Object.keys(presentation.titleFont.mapping).length, 4);
for (const c of titleChars) assert.ok(presentation.titleFont.mapping[c.codePointAt(0)] >= 1);
assert.equal(presentation.titleFont.mapping["通".codePointAt(0)], undefined);
assert.equal(presentation.titleFont.mapping["道".codePointAt(0)], undefined);
const allUiText = Object.values(presentation.ui).join("") + "--";
for (const c of allUiText) {
  if (c === "\n" || c === " " || c === "%" || c === "u" || c === "s") continue;
  assert.ok(presentation.font.mapping[c.codePointAt(0)], `Missing glyph in standard font: ${c}`);
}
const dummyScreen = new Uint8Array(1024 + 76800);
display.render(dummyScreen, { phase: 0, score: 0, walking: 0, turning: 0 });
const px = (x, y) => {
  const i = (y * 240 + x) * 4;
  return [display.frame.data[i], display.frame.data[i+1], display.frame.data[i+2]];
};
assert.ok(px(104, 145)[0] > 100); assert.ok(px(135, 146)[0] > 100);
assert.ok(px(103, 145)[0] < 20); assert.ok(px(136, 145)[0] < 20);
assert.ok(px(120, 144)[0] < 20); assert.ok(px(120, 147)[0] < 20);
function regionTextCount(startY, height, isDarkText = false) {
  let count = 0;
  for (let y = startY; y < startY + height; y++) {
    for (let x = 20; x < 220; x++) {
      const p = px(x, y);
      if (isDarkText ? p[0] < 180 : (p[0] > 20 || p[1] > 20 || p[2] > 20)) count++;
    }
  }
  return count;
}
assert.ok(regionTextCount(66, 20) > 100);
assert.ok(regionTextCount(160, 72) > 500);
assert.ok(regionTextCount(94, 28) > 200);
assert.ok(regionTextCount(252, 20) > 100);
assert.ok(regionTextCount(287, 20) > 100);
display.render(dummyScreen, { phase: 2, score: 8, walking: 0, turning: 0 });
assert.equal(regionTextCount(66, 20, true), 0);
assert.ok(regionTextCount(166, 20, true) > 100);
assert.ok(regionTextCount(94, 28, true) > 200);
assert.ok(regionTextCount(252, 20, true) > 100);
assert.ok(regionTextCount(287, 20, true) > 100);
let lastFadeR = 0;
for (const dt of [0, 200, 400, 600, 800, 1000, 2000]) {
  display.phase = -1;
  display.render(dummyScreen, { phase: 0, score: 0, walking: 0, turning: 0 }, false, 1000);
  display.render(dummyScreen, { phase: 0, score: 0, walking: 0, turning: 0 }, false, 1000 + dt);
  const r = px(84, 254)[0];
  assert.ok(r >= lastFadeR);
  if (dt >= 800) assert.equal(r, 231);
  lastFadeR = r;
}
display.blend(-1, 0, 255, 255, 255);
display.blend(240, 0, 255, 255, 255);
display.blend(0, -1, 255, 255, 255);
display.blend(0, 320, 255, 255, 255);
display.rect(-10, -10, 300, 400, [255, 255, 255]);
display.text("Out of bounds\nand unmapped: \u{1F600}", -50, -50, true);
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'corridor-web-test-'));
let changed=0,pixels=0,maxColorDelta=0,maxStateError=0;
try{
  const exe=path.join(dir,'native');
  let result=spawnSync(process.env.CC||'cc',['-O2','-ffp-contract=off','-std=c11','-Wall','-Wextra','-Werror','-Imain','tests/corridor_web_native.c','prototype/exit-corridor/firmware/bridge.c','main/corridor_game.c','main/corridor_render.c','-lm','-o',exe],{encoding:'utf8'});
  assert.equal(result.status,0,result.stderr);
  result=spawnSync(exe,[],{input:steps.join('\n')+'\n',maxBuffer:32*1024*1024});assert.equal(result.status,0,result.stderr?.toString());
  const stateBytes=STATE_FIELDS.length*8,recordSize=stateBytes+77824;
  assert.equal(result.stdout.length,recordSize*snapshots.length);
  for(let s=0;s<snapshots.length;s++){
    const base=s*recordSize,expected=snapshots[s];
    for(let f=0;f<STATE_FIELDS.length;f++){
      const native=result.stdout.readDoubleLE(base+f*8),web=expected.state[STATE_FIELDS[f]],delta=Math.abs(native-web);
      maxStateError=Math.max(maxStateError,delta);assert.ok(delta<.0002,`${s}/${STATE_FIELDS[f]}: native ${native}, Wasm ${web}`);
    }
    const image=result.stdout.subarray(base+stateBytes,base+recordSize);
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
