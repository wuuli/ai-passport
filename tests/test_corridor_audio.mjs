import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {FirmwareCore} from '../prototype/exit-corridor/firmware/runtime.mjs';
import {CorridorAudio} from '../prototype/exit-corridor/firmware/audio.mjs';
import {AUDIO_REVIEWS, renderAudioReview, encodeWav} from '../prototype/exit-corridor/firmware/audio-review.mjs';
const {instance}=await WebAssembly.instantiate(fs.readFileSync('prototype/exit-corridor/firmware/corridor.wasm'),{});
const core=new FirmwareCore(instance,fs.readFileSync('assets/images/exit-corridor/commuter-device.bin'),1);
const trace=[],chunks=[];
function command(line){
  trace.push(line);const [op,...args]=line.split(' '),n=args.map(Number);
  if(op==='E')core.audioEnabled(n[0]);else if(op==='K')core.key(n[0]);
  else if(op==='T')core.tick(n[0]);else if(op==='P')core.pause();
  else if(op==='H')core.title();else if(op==='R')core.review(...n);
  else if(op==='A')chunks.push(Buffer.from(core.audioRender(640).slice().buffer));
}
command('E 1');command('A');assert.ok(chunks.at(-1).every(x=>x===0));
command('K 2');command('K 2');
for(let i=0;i<80;i++){command('T 0.04');command('A');}
assert.ok(chunks.at(-1).some(x=>x!==0));
command('P');for(let i=0;i<10;i++){command('T 0.04');command('A');}
command('E 0');command('A');assert.ok(chunks.at(-1).every(x=>x===0));
command('E 1');command('A');
command('R 0 0 -10.9 0 8 0');command('K 2');
for(let i=0;i<80;i++){command('T 0.04');command('A');}
assert.equal(core.state().phase,2);assert.equal(core.audioRunning(),false);
command('K 2');command('A');command('H');command('A');
assert.ok(chunks.at(-1).every(x=>x===0));
assert.throws(()=>core.audioRender(2048),RangeError);
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'corridor-audio-'));
try{
  const exe=path.join(dir,'native');
  const build=spawnSync(process.env.CC||'cc',['-O2','-ffp-contract=off','-std=c11','-Wall','-Wextra','-Werror','-Imain',
    'tests/corridor_web_native.c','prototype/exit-corridor/firmware/bridge.c','main/corridor_game.c',
    'main/corridor_render.c','main/corridor_sound.c','-lm','-o',exe],{encoding:'utf8'});
  assert.equal(build.status,0,build.stderr);
  const run=spawnSync(exe,[],{input:trace.join('\n')+'\n',maxBuffer:4*1024*1024});
  assert.equal(run.status,0,run.stderr.toString());
  assert.deepEqual(run.stdout,Buffer.concat(chunks),'native and Wasm PCM must be identical');
}finally{fs.rmSync(dir,{recursive:true,force:true});}

// Exercise the shipped browser transport: audio gate, bounded scheduling,
// cancellation, resume rejection and an unlock completing after page disposal.
class Context{
  state='suspended';currentTime=0;destination={};made=[];
  async resume(){this.state='running';}
  async close(){this.state='closed';}
  createGain(){return {gain:{value:1,setTargetAtTime(value){this.value=value;}},connect(){},disconnect(){}};}
  createBuffer(ch,count,rate){assert.equal(rate,16000);return {getChannelData:()=>new Float32Array(count)};}
  createBufferSource(){const source={connect(){},disconnect(){},start(at){this.at=at;},stop(){this.stopped=true;}};this.made.push(source);return source;}
}
let scheduled=0,canceled=0;
const audio=new CorridorAudio(core,{Context,schedule:()=>++scheduled,cancel:()=>++canceled});
core.key(2);audio.setActive(true);assert.equal(core.audioRunning(),false);
await audio.unlock();assert.equal(core.audioRunning(),true);assert.equal(scheduled,1);
assert.equal(audio.volume,0.4);
assert.equal(audio.gain.gain.value,4); // Keep the previous full-scale listening level.
audio.setVolume(1);assert.equal(audio.gain.gain.value,10);
audio.setVolume(0);assert.equal(audio.gain.gain.value,0);
audio.setVolume(.4);
assert.equal(audio.gain.gain.value,4);
assert.ok(audio.sources.size>0&&audio.sources.size<=3);
const scheduledEnd=audio.next,previousCount=audio.context.made.length;
audio.context.currentTime=scheduledEnd-.002;audio.pump();
assert.equal(audio.context.made[previousCount].at,scheduledEnd,'late timer must not insert a gap ahead of queued audio');
assert.equal(audio.underruns,0);
audio.context.currentTime=audio.next+.05;audio.pump();
assert.equal(audio.underruns,1,'real starvation must be observable');
assert.ok(audio.maxGapMs>=49);
const old=[...audio.sources];audio.setActive(false);
assert.equal(core.audioRunning(),false);assert.ok(old.every(x=>x.stopped));assert.equal(audio.sources.size,0);
audio.setActive(true);audio.setEnabled(false);assert.equal(core.audioRunning(),false);
audio.setEnabled(true);audio.dispose();assert.equal(core.audioRunning(),false);assert.equal(audio.context.state,'closed');
assert.equal(scheduled,canceled);
const unavailable=new CorridorAudio(core,{Context:null});unavailable.setActive(true);await unavailable.unlock();
assert.equal(core.audioRunning(),false);unavailable.dispose();
class Rejected extends Context{async resume(){throw Error('blocked');}}
const rejected=new CorridorAudio(core,{Context:Rejected});rejected.setActive(true);await rejected.unlock();
assert.equal(rejected.enabled,false);assert.equal(core.audioRunning(),false);rejected.dispose();
let resume;
class Delayed extends Context{resume(){return new Promise(resolve=>{resume=()=>{this.state='running';resolve();};});}}
const delayed=new CorridorAudio(core,{Context:Delayed});delayed.setActive(true);
const unlocking=delayed.unlock();delayed.dispose();resume();await unlocking;
assert.equal(core.audioRunning(),false);assert.equal(delayed.sources.size,0);
const toggle=new CorridorAudio(core,{Context,schedule:()=>0,cancel:()=>{}});
assert.equal(toggle.buttonLabel(),'点击启用声音');
await toggle.activate();assert.equal(toggle.buttonLabel(),'声音：开');
assert.equal(toggle.enabled,true);
await toggle.activate();assert.equal(toggle.buttonLabel(),'声音：关');
await toggle.activate();assert.equal(toggle.buttonLabel(),'声音：开');
toggle.context.state='suspended';
assert.equal(toggle.buttonLabel(),'点击启用声音');
await toggle.activate();assert.equal(toggle.buttonLabel(),'声音：开');
toggle.dispose();
const reviews={};
for(const [kind,spec] of Object.entries(AUDIO_REVIEWS)){
  const result=reviews[kind]=renderAudioReview(core,kind);
  assert.equal(result.samples.length,spec.seconds*16000);
  assert.equal(result.clipped,0,`${kind}: 60% reference must not clip`);
  assert.ok(result.peak>0&&result.peak<1);
  const wav=encodeWav(result.samples),header=new DataView(wav.buffer);
  assert.equal(header.getUint32(24,true),16000);
  assert.equal(header.getUint32(40,true),result.samples.length*2);
  for(let i=0;i<result.samples.length;i++)assert.equal(header.getInt16(44+i*2,true),result.samples[i]);
}
assert.deepEqual(reviews.observing.samples,reviews.ambience.samples.slice(0,5*16000),'turning must not create footsteps');
assert.ok(reviews.ambience.rms < 10**(-42/20),'idle ambience at the 60% reference must stay below -42 dBFS RMS');
assert.ok(reviews.walking.peak > reviews.ambience.peak*4,'foot contacts must stand out from stationary ambience');
assert.ok(reviews.walking.events.some(event=>event.walking));
assert.ok(reviews.walking.events.filter(event=>event.seconds>=6).every(event=>!event.walking));
assert.ok(reviews.corner.events.some(event=>event.turning));
assert.ok(reviews.corner.events.at(-1).cornerStop);
assert.equal(reviews.completion.events.at(-1).phase,2);
assert.ok(reviews.completion.samples.slice(-16000).every(sample=>sample===0),'completion tail must end');
assert.ok(reviews.lifecycle.samples.slice(0,2*16000).every(sample=>sample===0),'title before entering must be silent');
assert.ok(reviews.lifecycle.samples.slice(2*16000,3*16000).some(sample=>sample!==0),'entering must start ambience within the first second');
assert.ok(reviews.lifecycle.events.filter(event=>event.seconds>=7&&event.seconds<10).every(event=>!event.walking));
assert.deepEqual(reviews.lifecycle.samples.slice(7.5*16000,8*16000),reviews.ambience.samples.slice(5.5*16000,6*16000),'stopped footsteps must decay back to ambience, never loop');
assert.ok(reviews.lifecycle.samples.slice(10*16000).every(sample=>sample===0),'returning to title must silence all subsequent PCM');
// Repeat the actual start/stop/exit path so old counters cannot latch a sound.
for(let i=0;i<30;++i){
  const again=renderAudioReview(core,'lifecycle');
  assert.deepEqual(again.samples,reviews.lifecycle.samples,`lifecycle repeat ${i}`);
}
core.destroy();
console.log(`Corridor audio: ${chunks.length*640} native/Wasm PCM samples identical; browser lifecycle PASS`);
