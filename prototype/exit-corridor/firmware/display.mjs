/* Browser display shell. Uses the exact generated firmware font bitmaps/copy.
 * RGB565 quantization mirrors the native image decoder; LVGL itself is not emulated. */
export class FirmwareDisplay {
  constructor(canvas, presentation) {
    this.canvas=canvas;canvas.width=240;canvas.height=320;
    this.context=canvas.getContext('2d',{alpha:false});
    if(!this.context)throw new Error('当前浏览器无法创建画布');
    this.frame=this.context.createImageData(240,320);
    this.font=presentation.font;this.ui=presentation.ui;
  }
  blend(x,y,r,g,b,alpha=255) {
    if(x<0||x>=240||y<0||y>=320)return;
    const i=(y*240+x)*4,p=this.frame.data,a=alpha/255;
    p[i]=Math.round(p[i]*(1-a)+r*a);p[i+1]=Math.round(p[i+1]*(1-a)+g*a);p[i+2]=Math.round(p[i+2]*(1-a)+b*a);p[i+3]=255;
  }
  rect(x,y,w,h,color,alpha=255) {for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)this.blend(i,j,...color,alpha);}
  width(text) {return [...text].reduce((sum,c)=>sum+Math.round((this.font.glyphs[this.font.mapping[c.codePointAt(0)]]?.adv_w??0)/16),0);}
  text(text,x,y,center=false,color=[239,238,232]) {
    for(const [lineIndex,line] of text.split('\n').entries()) {
      let pen=center?Math.round(x-this.width(line)/2):x;
      for(const char of line) {
        const glyph=this.font.glyphs[this.font.mapping[char.codePointAt(0)]];
        if(!glyph)continue;
        const top=y+lineIndex*this.font.lineHeight+this.font.lineHeight-this.font.baseline-glyph.box_h-glyph.ofs_y;
        for(let row=0;row<glyph.box_h;row++)for(let col=0;col<glyph.box_w;col++) {
          const bit=(row*glyph.box_w+col)*2;
          const alpha=((this.font.bitmap[glyph.bitmap_index+(bit>>3)]>>(6-(bit&7)))&3)*85;
          if(alpha)this.blend(pen+glyph.ofs_x+col,top+row,...color,alpha);
        }
        pen+=Math.round(glyph.adv_w/16);
      }
    }
  }
  render(indexed,state,paused=false) {
    const p=this.frame.data;
    for(let pixel=0;pixel<76800;pixel++) {
      const c=indexed[1024+pixel]*4,i=pixel*4;
      // Firmware scanout truncates its designed palette to RGB565.
      p[i]=Math.round((indexed[c+2]>>3)*255/31);
      p[i+1]=Math.round((indexed[c+1]>>2)*255/63);
      p[i+2]=Math.round((indexed[c]>>3)*255/31);p[i+3]=255;
    }
    this.rect(0,0,240,24,[20,25,24],204);
    const motion=state.turning&&state.walking?this.ui.turning:state.walking?this.ui.walking:this.ui.stopped;
    this.text(state.phase===1?this.ui.status.replace('%u',state.score).replace('%s',motion):this.ui.exit,7,4);
    this.text('--',233-this.width('--'),4);
    if(state.phase!==1) {
      this.rect(8,77,224,166,[23,28,27],230);
      const edge=[214,185,84];this.rect(8,77,224,1,edge);this.rect(8,242,224,1,edge);
      this.rect(8,77,1,166,edge);this.rect(231,77,1,166,edge);
      this.text(state.phase===0?this.ui.title:this.ui.win,120,101,true);
      this.text(state.phase===0?this.ui.goal:this.ui.cleared,120,144,true);
    }
    this.context.putImageData(this.frame,0,0);
    this.canvas.setAttribute('aria-label',state.phase===0?'地下通道，按 OK 进入':state.phase===2?'已走出通道，按 OK 再走一次':`出口 ${state.score}，${paused?'评审暂停':motion}`);
  }
}
