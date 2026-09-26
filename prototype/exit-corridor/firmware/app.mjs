import {loadFirmware} from './runtime.mjs';
import {FirmwareDisplay} from './display.mjs';
import {ThreeKeyInput} from './input.mjs';
const $=id=>document.getElementById(id),canvas=$('corridorCanvas'),buttons=[0,1,2].map(i=>$('btnAction'+(i+1)));
const anomalyNames=['正常','维修门缺失','海报眼睛','海报倒置','额外通风口','红灯','高大路人','停止凝视','路人缺席'];
let core,display,input,reviewRunning=false,lastTime=0,lastPerf=0,raf=0,disposed=false,frames=0,cpuMs=0;
function reviewPaused(){return $('reviewPanel').open&&!reviewRunning;}
function freshSeed(){return crypto.getRandomValues(new Uint32Array(1))[0];}
function present(){
  if(!core)return;
  const state=core.state();display.render(core.draw(),state,reviewPaused());
  $('reviewPaused').hidden=!reviewPaused();
  buttons[0].disabled=buttons[1].disabled=(state.phase!==1&&state.phase!==3)||reviewPaused();
  buttons[2].disabled=false;
  $('btnAction3').querySelector('span').textContent=state.phase===0?'开始游戏':state.phase===2?'再玩一次':state.walking?'停下':'行走';
  $('toggleReview').textContent=reviewRunning?'暂停观察':'继续观察';
  if($('reviewPanel').open){
    $('reviewState').textContent=`出口 ${state.score} · ${anomalyNames[state.anomaly]} · X ${state.x.toFixed(3)} / Z ${state.z.toFixed(3)} · ${(state.cameraYaw*180/Math.PI).toFixed(1)}° · ${state.turning?'圆弧转弯':state.observing?'观察对中':state.walking?'行走':'停止'}`;
    $('reviewJudgement').textContent=state.passages?`第 ${state.passages} 次 · ${anomalyNames[state.lastAnomaly]} · ${state.forward?'继续前进':'折返'} · ${state.correct?'正确':'错误'} · ${state.scoreBefore} → ${state.score}`:'尚未跨越通道边界';
  }
}
function reset(){input?.clear();reviewRunning=false;if($('reviewPanel').open){$('reviewPanel').open=false;return;}core.reset(freshSeed());lastTime=0;present();}
function action(key){
  if(reviewPaused()){
    if(key!==2)return;reviewRunning=true;
    const phase=core.state().phase;
    if(phase===0||phase===2)core.key(2);
    else if(!core.state().walking)core.key(2);
  }else core.key(key);
  present();
}
function frame(now){
  if(disposed)return;
  raf=requestAnimationFrame(frame);
  if(document.hidden){lastTime=0;return;}
  // The firmware requests at most 20 game frames/s. Delayed ticks retain its
  // existing 0.25 s clamp; tab return never fast-forwards a hidden game.
  if(lastTime&&now-lastTime<50)return;
  const dt=lastTime?(now-lastTime)/1000:0;lastTime=now;
  const start=performance.now();
  if(!reviewPaused())core.tick(dt);
  present();cpuMs+=performance.now()-start;++frames;
  if(now-lastPerf>=2000){
    $('reviewPerf').textContent=`网页 ${(frames*1000/(now-lastPerf)).toFixed(1)} 帧/秒 · CPU ${(cpuMs/frames).toFixed(2)} ms/帧 · 非真机性能`;
    frames=0;cpuMs=0;lastPerf=now;
  }
}
function clear(){input?.clear();lastTime=0;if(core)present();}
function setupInput(){
  input=new ThreeKeyInput({key:action,exit:()=>{core.title();present();},pause:()=>core.pause(),
    changed:hold=>buttons.forEach((button,key)=>button.classList.toggle('pressed',hold?.key===key))});
  for(const [key,button] of buttons.entries()){
    button.addEventListener('pointerdown',event=>{event.preventDefault();if(button.disabled)return;
      canvas.focus({preventScroll:true});button.setPointerCapture(event.pointerId);input.press('p'+event.pointerId,key);});
    button.addEventListener('pointerup',event=>{event.preventDefault();input.release('p'+event.pointerId);});
    for(const type of ['pointercancel','lostpointercapture'])button.addEventListener(type,event=>input.release('p'+event.pointerId,true));
    // Assistive-technology activation has no pointer press/release pair.
    button.addEventListener('click',event=>{if(event.detail===0&&!input.active&&!button.disabled){input.press('a'+key,key);input.release('a'+key);}});
  }
  const mapping={ArrowUp:0,ArrowDown:1,Space:2,Enter:2};
  window.addEventListener('keydown',event=>{
    if(event.target.closest?.('select,input,textarea,summary')||event.metaKey||event.ctrlKey||event.altKey)return;
    if(event.target.closest?.('button')&&!event.target.closest?.('.keys'))return;
    const key=mapping[event.code];if(key===undefined)return;event.preventDefault();
    if(!event.repeat&&!buttons[key].disabled)input.press('k'+event.code,key);
  });
  window.addEventListener('keyup',event=>{if(mapping[event.code]!==undefined){event.preventDefault();input.release('k'+event.code);}});
  window.addEventListener('blur',clear);document.addEventListener('visibilitychange',clear);
}
async function start(){
  try{
    const loaded=await loadFirmware();core=loaded.core;display=new FirmwareDisplay(canvas,loaded.presentation);
    setupInput();$('loading').hidden=true;
    for(const id of ['restart','applySample','toggleReview','blind'])$(id).disabled=false;
    const m=loaded.manifest;
    $('reviewBuild').textContent=`C 游戏／渲染 ${m.sources['main/corridor_game.c'].slice(0,10)} / ${m.sources['main/corridor_render.c'].slice(0,10)} · 人物 ${(m.spriteBytes/1024).toFixed(2)} KiB · 字体直接读取固件位图`;
    $('restart').addEventListener('click',reset);$('blind').addEventListener('click',reset);
    $('reviewPanel').addEventListener('toggle',()=>{clear();reviewRunning=false;if(!$('reviewPanel').open)core.reset(freshSeed());present();});
    const resume=()=>{reviewRunning=!reviewRunning;core.pause();lastTime=0;present();};
    $('toggleReview').addEventListener('click',resume);$('resumeReview').addEventListener('click',()=>{reviewRunning=true;lastTime=0;present();});
    $('applySample').addEventListener('click',()=>{
      clear();const fixture=$('fixture').value;
      const views={entry:[0,-1.8,0,0,0],signForward:[0,-10,0,0,0],signReturn:[0,-18,Math.PI,0,0],poster45:[0,-19.131,Math.PI/4,0,0],corner:[0,-25.1,0,0,0],
        northPortal:[4.7,-26.8,Math.PI/2,0,0],southPortal:[-4.7,2.8,-Math.PI/2,0,0],final:[4.7,-26.8,Math.PI/2,7,0],finalReturn:[-4.7,2.8,-Math.PI/2,7,0],
        exitApproach:[0,-3.5,0,8,0],exitStairs:[0,-6.8,0,8,0],exitStairsReturn:[0,-17.2,Math.PI,8,1]};
      core.review(Number($('anomaly').value),...views[fixture]);reviewRunning=false;present();
    });
    // Read-only status for diagnostics; review controls above are the only
    // browser fixture mutation route. No second JavaScript gameplay model.
    window.exitCorridor={ready:Promise.resolve(),getState:()=>core.state(),source:m};
    lastPerf=performance.now();present();raf=requestAnimationFrame(frame);
  }catch(error){console.error('Firmware preview initialization failed',error);$('loading').textContent=`加载失败：${error.message}。请刷新重试。`;}
}
$('scale').addEventListener('change',()=>{$('device').style.setProperty('--scale',$('scale').value);});
$('fullscreen').addEventListener('click',async()=>{try{await $('device').requestFullscreen();}catch(error){console.warn('Fullscreen unavailable',error);}});
$('exitFullscreen').addEventListener('click',()=>document.exitFullscreen());
window.addEventListener('pagehide',event=>{clear();if(!event.persisted){disposed=true;cancelAnimationFrame(raf);core?.destroy();}});
start();
