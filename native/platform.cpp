/* BBK 9588 platform services for the original Space Cadet engine. */
#include "engine/pch.h"
#include "engine/options.h"
#include "engine/fullscrn.h"
#include "engine/winmain.h"
#include "engine/pb.h"
#include "engine/midi.h"
#include "engine/high_score.h"
#include "engine/TPinballTable.h"
#include "engine/TTextBox.h"
#include "engine/Sound.h"
#include "platform.h"

char scp_error_text[256];
int scp_popup_requested=0;
extern "C" __attribute__((weak)) void scp_checkpoint(const char *stage){printf("%s\n",stage);}
optionsStruct options::Options;
bool winmain::single_step=false,winmain::LaunchBallEnabled=false,winmain::HighScoresEnabled=false,winmain::DemoActive=false;
SDL_Window* winmain::MainWindow=nullptr;
SDL_Renderer* winmain::Renderer=nullptr;
int winmain::MainMenuHeight=0;

std::vector<GameBindings> options::MapGameInput(GameInput input){
    std::vector<GameBindings> out;
    if(input.Type!=InputTypes::Keyboard)return out;
    switch(input.Value){
    case 1:out.push_back(GameBindings::LeftFlipper);break;
    case 2:out.push_back(GameBindings::RightFlipper);break;
    case 3:out.push_back(GameBindings::Plunger);break;
    case 4:out.push_back(GameBindings::BottomTableBump);break;
    }
    return out;
}
void winmain::memalloc_failure(){scp_runtime_panic("Out of memory");}
void winmain::new_game(){single_step=false;pb::replay_level(false);}
void winmain::pause(bool toggle){if(toggle)scp_popup_requested=!scp_popup_requested;else scp_popup_requested=1;}
void winmain::end_pause(){scp_popup_requested=0;}
void winmain::HandleGameBinding(GameBindings binding,bool){
    if(binding==GameBindings::NewGame)new_game();
    if(binding==GameBindings::TogglePause)pause();
}
int SDL_ShowSimpleMessageBox(Uint32,const char*title,const char*message,SDL_Window*){
    snprintf(scp_error_text,sizeof(scp_error_text),"%s: %s",title?title:"Data error",message?message:"");
    return 0;
}

int fullscrn::screen_mode=0,fullscrn::display_changed=0,fullscrn::resolution=0;
float fullscrn::ScaleX=1,fullscrn::ScaleY=1;
int fullscrn::OffsetX=0,fullscrn::OffsetY=0;
const resolution_info fullscrn::resolution_array[3]={{640,480,600,480,501},{800,600,752,600,502},{1024,768,960,768,503}};
int fullscrn::GetResolution(){return 0;}
void fullscrn::SetResolution(int){resolution=0;}
int fullscrn::GetMaxResolution(){return 0;}
float fullscrn::GetScreenToPinballRatio(){return 1;}
SDL_Rect fullscrn::GetScreenRectFromPinballRect(SDL_Rect rect){return rect;}
void fullscrn::init(){}
void fullscrn::shutdown(){}
int fullscrn::set_screen_mode(int){return 0;}
void fullscrn::activate(int){}
void fullscrn::window_size_changed(){}

/* This data set contains the original effects. No synthesized substitute for MIDI. */
int midi::music_init(bool,int){return 0;}
void midi::music_shutdown(){}
void midi::music_play(){}
void midi::music_stop(){}
void midi::SetVolume(int){}
bool midi::play_track(MidiTracks,bool){return false;}
MidiTracks midi::get_active_track(){return MidiTracks::None;}

high_score_struct high_score::highscore_table[5];
static unsigned best_score=0,saved_best=0,save_sequence=0;
static unsigned checksum(const unsigned*data,unsigned count){
    unsigned hash=2166136261u;
    for(unsigned i=0;i<count;++i){hash^=data[i];hash*=16777619u;}
    return hash;
}
static std::string save_path(unsigned slot){return pb::make_path_name(slot?"SCORE1.SAV":"SCORE0.SAV");}
int high_score::read(){
    best_score=saved_best=save_sequence=0;
    memset(highscore_table,0,sizeof(highscore_table));
    for(unsigned slot=0;slot<2;++slot){
        unsigned record[5];FILE*f=fopen(save_path(slot).c_str(),"rb");
        if(!f)continue;
        bool ok=fread(record,1,sizeof(record),f)==sizeof(record);
        fclose(f);
        if(ok&&record[0]==0x53435031u&&record[1]==1&&record[2]<=1000000000u&&record[4]==checksum(record,4)){
            if(record[2]>best_score)best_score=record[2];
            if((int)(record[3]-save_sequence)>0)save_sequence=record[3];
        }
    }
    saved_best=best_score;highscore_table[0].Score=(int)best_score;
    strcpy(highscore_table[0].Name,"PLAYER 1");return 0;
}
unsigned scp_platform_best(){return best_score;}
void scp_platform_update_best(){
    if(pb::MainTable&&pb::MainTable->CurScore>0&&(unsigned)pb::MainTable->CurScore>best_score)
        best_score=(unsigned)pb::MainTable->CurScore;
    for(unsigned i=0;i<5;++i)if(high_score::highscore_table[i].Score>0&&(unsigned)high_score::highscore_table[i].Score>best_score)
        best_score=(unsigned)high_score::highscore_table[i].Score;
}
int high_score::write(){
    scp_platform_update_best();if(best_score<=saved_best)return 0;
    unsigned record[5]={0x53435031u,1,best_score,save_sequence+1,0},verify[5];
    record[4]=checksum(record,4);
    std::string path=save_path(record[3]&1u);FILE*f=fopen(path.c_str(),"wb");
    if(!f)return -1;
    bool ok=fwrite(record,1,sizeof(record),f)==sizeof(record);int closed=fclose(f);
    if(!ok||closed!=0)return -1;
    f=fopen(path.c_str(),"rb");if(!f)return -1;
    ok=fread(verify,1,sizeof(verify),f)==sizeof(verify)&&memcmp(record,verify,sizeof(record))==0;
    closed=fclose(f);if(!ok||closed!=0)return -1;
    save_sequence=record[3];saved_best=best_score;return 0;
}
void scp_platform_save_best(){high_score::write();}
int high_score::get_score_position(int score){
    for(int i=0;i<5;++i)if(score>highscore_table[i].Score)return i;
    return -1;
}
void high_score::show_high_score_dialog(){scp_popup_requested=1;}
void high_score::show_and_set_high_score_dialog(high_score_entry entry){
    int pos=get_score_position(entry.Entry.Score);if(pos>=0){
        for(int i=4;i>pos;--i)highscore_table[i]=highscore_table[i-1];
        highscore_table[pos]=entry.Entry;strcpy(highscore_table[pos].Name,"PLAYER 1");
    }
    scp_platform_update_best();write();scp_popup_requested=1;
}
void high_score::RenderHighScoreDialog(){}
