/* Native BBK 9588 window, portrait presentation and input for Space Cadet. */
#include "engine/pch.h"
#include "engine/pb.h"
#include "engine/options.h"
#include "engine/winmain.h"
#include "engine/TPinballTable.h"
#include "engine/TBall.h"
#include "engine/Sound.h"
#include "bda_graphics.h"
#include "bda_window.h"
#include "bda_input.h"
#include "bda_time.h"
#include "bda_memory.h"
#include "bda_audio.h"
#include "bda_dialogs.h"
#include "platform.h"
#include "ui.h"
#include "runtime.h"

extern "C" int scp_on_stack(void*,int(*)(void));
extern "C" int scp_setjmp(void*) __attribute__((returns_twice));
extern "C" void scp_longjmp(void*,int) __attribute__((noreturn));
extern "C" char scp_runtime_log[8192];
static unsigned panic_context[12];
static volatile int can_jump;
static bda_handle_t frame,draw,back,draw_owner;
static void *brush;
static volatile int detached,dirty,touch_active;
static volatile unsigned touch_bits;
static struct { unsigned char header[24];unsigned short pixels[240*320]; } vx_frame;
static int popup,engine_ready,timer_active,audio_active;
static int ball_reported;
static unsigned previous_input,engine_input,audio_due,audio_fraction;
static short audio_buffer[512];
static int escape_suppressed,escape_pending;
static unsigned escape_due;

static void write_log(){FILE*f=fopen("A:/SCADLOG.TXT","wb");if(f){fwrite(scp_runtime_log,1,strlen(scp_runtime_log),f);fclose(f);}}
extern "C" void scp_checkpoint(const char *stage){printf("%s\n",stage);write_log();}
extern "C" void scp_runtime_panic(const char *message){
    snprintf(scp_error_text,256,"Engine: %s",message?message:"unknown error");
    printf("PANIC: %s\n",message?message:"");write_log();
    if(can_jump)scp_longjmp(panic_context,1);
    bda_msgbox("Space Cadet",scp_error_text);
    for(;;)bda_sys_delay(1);
}
static void release_draw(){if(draw&&(s32)draw!=-1)bda_gui_end_draw(draw);draw=draw_owner=0;}
static void acquire_draw(bda_handle_t owner){
    if(draw&&draw_owner==owner)return;release_draw();draw=bda_gui_current_draw(owner);
    if((s32)draw==-1)draw=0;draw_owner=owner;
}
static void suppress_escape(){escape_suppressed=1;escape_pending=0;}
/* Bit0/1 flippers, bit2 plunger, bit3 table nudge, bit4 scoreboard. */
static unsigned touch_control(unsigned lp){
    int x=(short)lp,y=(short)(lp>>16);if(x<0||x>=240||y<0||y>=320)return 0;
    if(y<22)return 16;
    if(popup)return y>285?32:0;
    if(y>=294){if(x<76)return 1;if(x>=164)return 2;return 4;}
    if(y>=205)return x<120?1:2;
    return 8;
}
static int window_proc(bda_handle_t h,u32 msg,u32 wp,u32 lp){
    if(msg==BDA_MSG_DRAW_CONTEXT_ATTACH){acquire_draw(h);dirty=1;}
    if(msg==BDA_MSG_DRAW_CONTEXT_DETACH){release_draw();detached=1;}
    if(msg==BDA_MSG_REDRAW_INPUT)dirty=1;
    if(msg==0x21)suppress_escape();
    if(msg==BDA_MSG_TOUCH_COORDINATE){touch_active=1;touch_bits=touch_control(lp);suppress_escape();return 1;}
    if(msg==BDA_MSG_TOUCH_RELEASE){touch_active=0;touch_bits=0;suppress_escape();return 1;}
    return bda_gui_default_proc(h,msg,wp,lp);
}
static unsigned sample_input(unsigned now,int *escape){
    bda_gui_input_packet_t packet;unsigned bits=touch_bits;int esc;
    bda_gui_input_packet(&packet);
    if(packet.bytes[BDA_INPUT_PACKET_LEFT_INDEX])bits|=1;
    if(packet.bytes[BDA_INPUT_PACKET_RIGHT_INDEX])bits|=2;
    if(packet.bytes[BDA_INPUT_PACKET_ENTER_INDEX])bits|=4;
    if(packet.bytes[BDA_INPUT_PACKET_DOWN_INDEX])bits|=8;
    if(packet.bytes[BDA_INPUT_PACKET_UP_INDEX])bits|=16;
    esc=packet.bytes[BDA_INPUT_PACKET_ESCAPE_INDEX]!=0;
    if(escape_suppressed){if(!touch_active&&!esc)escape_suppressed=0;escape_pending=0;}
    else if(esc&&!escape_pending){escape_pending=1;escape_due=now+100;}
    if(escape_pending&&!esc&&(s32)(now-escape_due)>=0){escape_pending=0;*escape=1;}
    return bits;
}
static void release_game_inputs(){
    for(unsigned i=0;i<4;++i)if(engine_input&(1u<<i))pb::InputUp(GameInput(InputTypes::Keyboard,(int)i+1));
    engine_input=0;
}
static void set_popup(int show){
    if(popup==show)return;
    if(show){release_game_inputs();Sound::Deactivate();scp_platform_save_best();}
    else Sound::Activate();
    popup=show;scp_popup_requested=show;dirty=1;
}
static void process_input(unsigned input,int escape,int *quit){
    unsigned edges=input&~previous_input;previous_input=input;
    if(!engine_ready){if(escape)*quit=1;return;}
    if((edges&16)||escape||(popup&&(edges&(4|32)))){
        if(popup){set_popup(0);if((edges&4)&&pb::game_mode==GameModes::GameOver)winmain::new_game();}
        else if(escape){*quit=1;return;}else set_popup(1);
        return;
    }
    if(popup)return;
    if((edges&4)&&pb::game_mode==GameModes::GameOver){winmain::new_game();engine_input=0;return;}
    unsigned current=input&15u;
    for(unsigned i=0;i<4;++i){unsigned bit=1u<<i;
        if((current&bit)&&!(engine_input&bit))pb::InputDown(GameInput(InputTypes::Keyboard,(int)i+1));
        if(!(current&bit)&&(engine_input&bit))pb::InputUp(GameInput(InputTypes::Keyboard,(int)i+1));}
    engine_input=current;
}
static int present(){
    if(!draw||!back)return 0;
    if(engine_ready)scp_render_ui(vx_frame.pixels,popup);
    else scp_render_notice(vx_frame.pixels,"SPACE CADET",scp_error_text[0]?scp_error_text:"Loading original game data...");
    if(bda_gui_draw_vx(back,0,0,&vx_frame)!=0)return 0;
    bda_gui_draw_guard_begin();void *old=bda_gui_select_draw_object(draw,brush);
    int result=bda_gui_context_copy(back,0,0,240,320,draw,0,0,BDA_GUI_COLOR_KEY_BLACK_RGB565);
    bda_gui_select_draw_object(draw,old);bda_gui_draw_guard_end();dirty=0;return result==0;
}
static void audio_pump(unsigned now){
    if(!audio_active||(s32)(now-audio_due)<0||!bda_audio_ready())return;
    scp_audio_mix(audio_buffer,512);
    if(bda_audio_write(audio_buffer,sizeof(audio_buffer))!=sizeof(audio_buffer)){
        bda_audio_stop();audio_active=0;return;}
    audio_due+=23;audio_fraction+=4850;if(audio_fraction>=22050){++audio_due;audio_fraction-=22050;}
    if((s32)(now-audio_due)>46)audio_due=now+23;
}
static int find_data(){
    static const char *paths[]={
        "B:/\xd3\xa6\xd3\xc3/\xca\xfd\xbe\xdd/\xd3\xce\xcf\xb7/Pinball/",
        "A:/\xd3\xa6\xd3\xc3/\xca\xfd\xbe\xdd/\xd3\xce\xcf\xb7/Pinball/",
        "A:/Pinball/","B:/Pinball/"};
    for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i){
        pb::BasePath=paths[i];
        if(scp_resource_mount(pb::make_path_name("PINBALL.PAK").c_str()))scp_checkpoint("ORIGINAL RESOURCE PACK MOUNTED");
        FILE*f=fopen(pb::make_path_name("PINBALL.DAT").c_str(),"rb");
        if(f){char sig[20]={0};bool ok=fread(sig,1,19,f)==19&&memcmp(sig,"PARTOUT(4.0)RESOURCE",19)==0;fclose(f);
            if(ok){pb::DatFileName="PINBALL.DAT";return 1;}}
    }
    strcpy(scp_error_text,"Original PINBALL.DAT missing. Copy the Pinball data folder to A:/application data/game/Pinball, or A:/Pinball.");return 0;
}
static int native_main(){
    bda_frame_desc_t desc;bda_gui_message_t message;
    static int closing,wait,quit;static unsigned last,render_due,accumulator;
    closing=wait=quit=0;last=render_due=accumulator=0;
    frame=draw=back=draw_owner=0;detached=touch_active=0;touch_bits=0;dirty=1;popup=0;engine_ready=timer_active=audio_active=0;ball_reported=0;
    previous_input=engine_input=0;escape_suppressed=escape_pending=0;scp_error_text[0]=0;scp_popup_requested=0;
    memset(&desc,0,sizeof(desc));memset(&message,0,sizeof(message));memset(&vx_frame,0,sizeof(vx_frame));
    vx_frame.header[0]='V';vx_frame.header[1]='X';
    for(int i=2;i<6;++i)vx_frame.header[i]=204;for(int i=14;i<20;++i)vx_frame.header[i]=204;
    for(int i=20;i<24;++i)vx_frame.header[i]=255;vx_frame.header[6]=240;vx_frame.header[10]=64;vx_frame.header[11]=1;
    desc.title="SPACE CADET";desc.wndproc=window_proc;desc.height=240;desc.width=320;
    frame=bda_gui_register_frame_desc(&desc);if(!frame||(s32)frame==-1)return 1;
    bda_gui_frame_activate(frame,0x100);acquire_draw(frame);brush=bda_gui_draw_object_create(7);
    if(!draw||!brush||(s32)(u32)brush==-1)goto cleanup;
    back=bda_gui_compatible_context_create(draw);if(!back||(s32)back==-1){back=0;goto cleanup;}
    present();printf("START ORIGINAL SPACE CADET\n");write_log();
    if(scp_setjmp(panic_context)){
        can_jump=0;engine_ready=0;dirty=1;closing=wait=quit=0;accumulator=0;
        if(audio_active){bda_audio_stop();audio_active=0;}
        if(timer_active){bda_gui_millisecond_timer_stop();timer_active=0;}
    }else{
        can_jump=1;
        if(find_data()){
            srand(bda_gui_tick_count_25ms());Sound::Init(true,8,true,128);
            printf("BEFORE PB INIT\n");write_log();
            if(pb::init()==0){engine_ready=1;pb::reset_table();pb::firsttime_setup();pb::replay_level(false);
                printf("READY TABLE %dx%d OFFSET %d,%d\n",pb::MainTable->Width,pb::MainTable->Height,pb::MainTable->XOffset,pb::MainTable->YOffset);write_log();}
            else strcpy(scp_error_text,"Cannot load original game data.");
        }
    }
    bda_gui_millisecond_timer_start();timer_active=1;last=bda_gui_millisecond_count();render_due=last;
    if(engine_ready){bda_audio_open_pcm(22050,16,1);audio_active=1;audio_due=last;audio_fraction=0;}
    present();
    while(!detached){
        int pump=bda_gui_event_pump_frame_once(&message,frame);
        if(!closing){
            unsigned now=bda_gui_millisecond_count(),elapsed=now-last;int escape=0;
            last=now;if(elapsed>100)elapsed=100;
            unsigned input=sample_input(now,&escape);process_input(input,escape,&quit);
            if(engine_ready&&!popup){
                accumulator+=elapsed*120;
                while(accumulator>=1000){pb::frame(1000.0f/120.0f);accumulator-=1000;}
                if(!ball_reported){for(auto ball:pb::MainTable->BallList)if(ball->ActiveFlag){
                    ball_reported=1;printf("BALL_READY TICK %d\n",pb::time_ticks);write_log();break;}}
                scp_platform_update_best();
                if(scp_popup_requested)set_popup(1);
            }else accumulator=0;
            audio_pump(now);
            if(dirty||(s32)(now-render_due)>=0){present();render_due=now+25;}
            if(quit){if(engine_ready){release_game_inputs();scp_platform_save_best();}
                can_jump=0;bda_gui_frame_stop(frame);bda_gui_frame_release(frame);closing=1;}
        }else if(!pump||++wait>=128)break;
        bda_sys_delay(1);
    }
cleanup:
    can_jump=0;
    if(audio_active){bda_audio_stop();audio_active=0;}
    if(timer_active){bda_gui_millisecond_timer_stop();timer_active=0;}
    if(engine_ready){pb::uninit();engine_ready=0;}Sound::Close();scp_resource_unmount();write_log();
    if(!closing){bda_gui_frame_stop(frame);bda_gui_frame_release(frame);wait=0;
        while(!detached&&bda_gui_event_pump_frame_once(&message,frame)&&++wait<128)bda_sys_delay(1);}
    if(back)bda_gui_compatible_context_free(back);release_draw();bda_gui_close_frame(frame);return 0;
}
extern "C" __attribute__((section(".text.bda_main"))) int bda_main(){
    void *stack=bda_alloc(512*1024);if(!stack||(s32)(u32)stack==-1)return 1;
    int result=scp_on_stack((char*)stack+512*1024,native_main);bda_free(stack);return result;
}
