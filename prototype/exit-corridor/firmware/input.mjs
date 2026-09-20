/* One ADC-style key at a time. Directions act on press; OK on short release. */
export class ThreeKeyInput {
  constructor({key,exit,pause,changed=()=>{},delay=(fn,ms)=>setTimeout(fn,ms),cancel=id=>clearTimeout(id)}) {
    Object.assign(this,{key,exit,pause,changed,delay,cancel});this.active=null;
  }
  press(source,key) {
    if(this.active||![0,1,2].includes(key))return false;
    const hold=this.active={source,key,long:false,timer:null};
    if(key===2)hold.timer=this.delay(()=>{if(this.active===hold){hold.long=true;this.exit();this.changed(this.active);}},1000);
    else this.key(key);
    this.changed(this.active);return true;
  }
  release(source,cancelled=false) {
    const hold=this.active;if(!hold||hold.source!==source)return;
    this.active=null;if(hold.timer!==null)this.cancel(hold.timer);
    if(cancelled)this.pause();else if(hold.key===2&&!hold.long)this.key(2);
    this.changed(null);
  }
  clear() {if(this.active)this.release(this.active.source,true);this.pause();}
}
