/* Original-engine host integration using the 9588 platform and portrait UI. */
#include "engine/pch.h"
#include "engine/pb.h"
#include "engine/render.h"
#include "engine/options.h"
#include "engine/Sound.h"
#include "engine/TPinballTable.h"
#include "engine/TBall.h"
#include "engine/TFlipper.h"
#include "engine/TPlunger.h"
#include "engine/TTextBox.h"
#include "engine/GroupData.h"
#include "engine/loader.h"
#include "engine/high_score.h"
#include "platform.h"
#include "ui.h"

extern "C" void scp_runtime_panic(const char* message) {
    std::fprintf(stderr,"Native engine panic: %s\n",message);std::abort();
}
extern "C" void scp_checkpoint(const char* stage) { std::printf("%s\n",stage); }
static unsigned short screen[240*320];
static void frame(const char* directory,const char* name,int popup) {
    char path[1024];std::snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    scp_render_ui(screen,popup);auto file=std::fopen(path,"wb");assert(file);
    std::fprintf(file,"P6\n240 320\n255\n");
    for(auto c:screen){std::fputc(((c>>11)&31)*255/31,file);std::fputc(((c>>5)&63)*255/63,file);std::fputc((c&31)*255/31,file);}
    std::fclose(file);
}
static void step(unsigned frames) {for(unsigned i=0;i<frames;++i)pb::frame(1000.f/120.f);}
static void input(int key,bool down) {
    GameInput value(InputTypes::Keyboard,key);
    if(down)pb::InputDown(value);else pb::InputUp(value);
}
static void ball_state(const char* name) {
    auto b=pb::MainTable->BallList[0];auto p=pb::MainTable->Plunger;
    std::printf("%s ball=(%.3f,%.3f,%.3f) speed=%.3f radius=%.3f active=%d mask=%d plunger=(%.3f,%.3f) boost=%.3f threshold=%.3f\n",name,
        double(b->Position.X),double(b->Position.Y),double(b->Position.Z),double(b->Speed),double(b->Radius),int(b->ActiveFlag),b->CollisionMask,
        double(pb::MainTable->PlungerPosition.X),double(pb::MainTable->PlungerPosition.Y),double(p->Boost),double(p->Threshold));
}
int main(int argc,char**argv) {
    const char* data=argc>1?argv[1]:"pinball-9588/original-data";
    const char* out=argc>2?argv[2]:"pinball-9588/qa";
    pb::BasePath=std::string(data)+"/";pb::DatFileName="PINBALL.DAT";
    pb::FullTiltMode=false;pb::FullTiltDemoMode=false;
    options::Options.Sounds=true;options::Options.Music=false;options::Options.Players=1;
    Sound::Init(true,8,true,128);
    assert(pb::init()==0);pb::firsttime_setup();
    assert(pb::MainTable&&render::vscreen);
    auto table=pb::MainTable;
    std::printf("Original engine: table=%dx%d sourceoffset=%d,%d vscreen=%dx%d components=%zu balls=%zu\n",
        table->Width,table->Height,table->XOffset,table->YOffset,
        render::vscreen->Width,render::vscreen->Height,table->ComponentList.size(),table->BallList.size());
    assert(table->Width==365&&table->Height==470);
    assert(render::vscreen->Height>=table->Height);
    pb::replay_level(false);
    unsigned feedFrames=0;
    while(!table->BallList[0]->ActiveFlag&&feedFrames<1200){step(1);++feedFrames;}
    assert(table->BallList[0]->ActiveFlag);
    step(120);std::printf("Original opening feed: %u frames at120Hz\n",feedFrames);
    frame(out,"original-ready",0);ball_state("Ready");
    input(1,true);input(2,true);step(24);
    assert(table->FlipperL->BmpIndex>0&&table->FlipperR->BmpIndex>0);
    frame(out,"original-flippers",0);
    input(1,false);input(2,false);step(24);
    assert(table->FlipperL->BmpIndex==0&&table->FlipperR->BmpIndex==0);
    auto ball=table->BallList[0];auto before=ball->Position;
    input(3,true);step(360);frame(out,"original-charge",0);
    std::printf("Plunger: %zu sprite frames, increment=%.3f boost=%.3f max=%.3f\n",table->Plunger->ListBitmap->size(),
        double(table->Plunger->PullbackIncrement),double(table->Plunger->Boost),double(table->Plunger->MaxPullback));
    ball_state("Charged");input(3,false);ball_state("Release");
    float maxSpeed=0;unsigned maxScore=0,positionChanges=0;bool left=false,right=false;
    for(unsigned i=0;i<2400;++i){
        bool newLeft=((i/41)%2)==0,newRight=((i/57)%2)==0;
        if(newLeft!=left){input(1,newLeft);left=newLeft;}
        if(newRight!=right){input(2,newRight);right=newRight;}
        pb::frame(1000.f/120.f);
        if(i<6)ball_state("Post-release");
        if(ball->Speed>maxSpeed)maxSpeed=ball->Speed;
        if(table->CurScore>0&&unsigned(table->CurScore)>maxScore)maxScore=unsigned(table->CurScore);
        if(ball->Position.X!=before.X||ball->Position.Y!=before.Y)++positionChanges;
        before=ball->Position;
        if(i==120)frame(out,"original-action",0);
        if(i==600)frame(out,"original-action-5s",0);
    }
    input(1,false);input(2,false);
    assert(maxSpeed>50&&positionChanges>120&&maxScore>0);
    frame(out,"original-popup",1);
    auto frozenTicks=pb::time_ticks;auto frozenPosition=ball->Position;
    for(unsigned i=0;i<120;++i)scp_render_ui(screen,1);
    assert(pb::time_ticks==frozenTicks&&ball->Position.X==frozenPosition.X&&ball->Position.Y==frozenPosition.Y);
    scp_platform_update_best();unsigned best=scp_platform_best();assert(high_score::write()==0);
    high_score::read();assert(scp_platform_best()==best&&best>=maxScore);
    std::printf("Original gameplay: maxspeed=%.3f movedframes=%u maxscore=%u current=%d best=%u ballcount=%d mode=%d\n",
        double(maxSpeed),positionChanges,maxScore,table->CurScore,best,table->BallCount,int(pb::game_mode));
    if(pb::InfoTextBox&&pb::InfoTextBox->CurrentMessage)std::printf("Info: %s\n",pb::InfoTextBox->CurrentMessage->Text);
    if(pb::MissTextBox&&pb::MissTextBox->CurrentMessage)std::printf("Mission: %s\n",pb::MissTextBox->CurrentMessage->Text);
    std::printf("HOST_RESULT {\"result\":\"PASS\",\"feed_frames_120hz\":%u,\"table_width\":%d,\"table_height\":%d,\"score\":%u,\"best\":%u,\"ball_count\":%d,\"max_speed\":%.3f,\"moving_frames\":%u}\n",
        feedFrames,table->Width,table->Height,maxScore,best,table->BallCount,double(maxSpeed),positionChanges);
    short samples[512];scp_audio_mix(samples,512);
    pb::uninit();Sound::Close();
    std::puts("Original engine bootstrap, flippers, plunger, moving-ball simulation, pause-popup and teardown passed");
    return 0;
}
