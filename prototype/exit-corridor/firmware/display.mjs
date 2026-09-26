/* Browser display shell. Uses the exact generated firmware font bitmaps/copy.
 * RGB565 quantization mirrors the native image decoder; LVGL itself is not emulated. */
export class FirmwareDisplay {
  constructor(canvas, presentation) {
    this.canvas=canvas;canvas.width=240;canvas.height=320;
    this.context=canvas.getContext('2d',{alpha:false});
    if(!this.context)throw new Error('当前浏览器无法创建画布');
    this.frame=this.context.createImageData(240,320);
    this.font=presentation.font;this.titleFont=presentation.titleFont;this.ui=presentation.ui;
    this.phase=-1;this.presentationStarted=0;
  }
  blend(x,y,r,g,b,alpha=255) {
    if(x<0||x>=240||y<0||y>=320)return;
    const i=(y*240+x)*4,p=this.frame.data,a=alpha/255;
    p[i]=Math.round(p[i]*(1-a)+r*a);p[i+1]=Math.round(p[i+1]*(1-a)+g*a);p[i+2]=Math.round(p[i+2]*(1-a)+b*a);p[i+3]=255;
  }
  rect(x,y,w,h,color,alpha=255) {for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)this.blend(i,j,...color,alpha);}
  width(text,font=this.font) {return [...text].reduce((sum,c)=>sum+Math.round((font.glyphs[font.mapping[c.codePointAt(0)]]?.adv_w??0)/16),0);}
  text(text,x,y,center=false,color=[239,238,232],opacity=255,font=this.font) {
    for(const [lineIndex,line] of text.split('\n').entries()) {
      let pen=center?Math.round(x-this.width(line,font)/2):x;
      for(const char of line) {
        const glyph=font.glyphs[font.mapping[char.codePointAt(0)]];
        if(!glyph)continue;
        const top=y+lineIndex*font.lineHeight+font.lineHeight-font.baseline-glyph.box_h-glyph.ofs_y;
        for(let row=0;row<glyph.box_h;row++)for(let col=0;col<glyph.box_w;col++) {
          const bit=(row*glyph.box_w+col)*2;
          const alpha=((font.bitmap[glyph.bitmap_index+(bit>>3)]>>(6-(bit&7)))&3)*85;
          const blended=Math.round(alpha*opacity/255);
          if(blended)this.blend(pen+glyph.ofs_x+col,top+row,...color,blended);
        }
        pen+=Math.round(glyph.adv_w/16);
      }
    }
  }
  render(indexed,state,paused=false,now=performance.now()) {
    if(state.phase!==this.phase){this.phase=state.phase;this.presentationStarted=now;}
    const p=this.frame.data;
    for(let pixel=0;pixel<76800;pixel++) {
      const c=indexed[1024+pixel]*4,i=pixel*4;
      // Firmware scanout truncates its designed palette to RGB565.
      p[i]=Math.round((indexed[c+2]>>3)*255/31);
      p[i+1]=Math.round((indexed[c+1]>>2)*255/63);
      p[i+2]=Math.round((indexed[c]>>3)*255/31);p[i+3]=255;
    }
    const motion=state.turning&&state.walking?this.ui.turning:state.walking?this.ui.walking:this.ui.stopped;
    const inWorld=state.phase===1||state.phase===3;
    const end=state.phase===2;
    const ink=end?[36,43,43]:[239,238,232];
    if(!inWorld) {
      this.rect(0,0,240,320,end?[243,244,240]:[8,13,13],end?230:178);
      const gold=[214,185,84],muted=end?[88,97,97]:[182,189,189];
      this.text(state.phase===0?this.ui.subtitle:'',120,66,true,muted);
      this.text(state.phase===0?this.ui.title:this.ui.win,120,94,true,ink,255,this.titleFont);
      this.rect(104,145,32,2,end?[140,150,148]:gold,153);
      this.text(state.phase===0?this.ui.guide:this.ui.cleared,120,state.phase===0?160:166,true,muted);
      const t=Math.min(1,Math.max(0,(now-this.presentationStarted)/800));
      this.text(state.phase===0?this.ui.start:this.ui.replay,120,252,true,end?ink:[231,205,112],Math.floor(153+102*t*t*(3-2*t)));
      this.text(this.ui.exit,120,287,true,muted);
    }
    if(inWorld){
      this.rect(0,0,240,24,[20,25,24],204);
      this.text(this.ui.status.replace('%u',state.hudScore).replace('%s',motion),7,4);
    }
    this.text('--',233-this.width('--'),4,false,ink);
    this.context.putImageData(this.frame,0,0);
    this.canvas.setAttribute('aria-label',state.phase===0?`8号出口，${this.ui.guide.replaceAll('\n','，')}，按 OK 进入`:state.phase===2?'已走出通道，按 OK 再走一次':`出口 ${state.hudScore}，${paused?'评审暂停':motion}`);
  }
}
