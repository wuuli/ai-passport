/* Test protocol: run the same trace through the native and Wasm bridge. */
#include "corridor_render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
uint8_t *web_sprite(void);unsigned web_sprite_size(void);int web_init(uint32_t);
void web_key(int);void web_tick(float);void web_pause(void);void web_title(void);
uint8_t *web_draw(void);void web_destroy(void);double web_state(unsigned);
int web_review(int,float,float,float,unsigned,int);
int main(void){
    FILE*f=fopen("assets/images/exit-corridor/commuter-device.bin","rb");assert(f);
    assert(fread(web_sprite(),1,web_sprite_size(),f)==web_sprite_size());fclose(f);assert(web_init(1));
    char line[256];
    while(fgets(line,sizeof(line),stdin)){
        int key,a,entry;unsigned seed,score;float dt,x,z,yaw;
        switch(line[0]){
        case 'I':assert(sscanf(line+1,"%u",&seed)==1);assert(web_init(seed));break;
        case 'K':assert(sscanf(line+1,"%d",&key)==1);web_key(key);break;
        case 'T':assert(sscanf(line+1,"%f",&dt)==1);web_tick(dt);break;
        case 'P':web_pause();break;case 'H':web_title();break;
        case 'R':assert(sscanf(line+1,"%d %f %f %f %u %d",&a,&x,&z,&yaw,&score,&entry)==6);assert(web_review(a,x,z,yaw,score,entry));break;
        case 'S':for(unsigned i=0;i<27;i++){double d=web_state(i);assert(fwrite(&d,sizeof(d),1,stdout)==1);}
            assert(fwrite(web_draw(),1,EC_IMAGE_BYTES,stdout)==EC_IMAGE_BYTES);break;
        default:abort();
        }
    }
    web_destroy();return 0;
}
