#pragma once
enum class InputTypes { None,Keyboard,Mouse,GameController };
struct GameInput {
    InputTypes Type; int Value;
    GameInput():Type(InputTypes::None),Value(-1){}
    GameInput(InputTypes type,int value):Type(type),Value(value){}
    bool operator==(const GameInput& v)const{return Type==v.Type&&Value==v.Value;}
};
enum class GameBindings {
    Min=0,LeftFlipper=0,RightFlipper,Plunger,LeftTableBump,RightTableBump,BottomTableBump,
    NewGame,TogglePause,ToggleFullScreen,ToggleSounds,ToggleMusic,ShowControlDialog,ToggleMenuDisplay,Exit,Max
};
struct optionsStruct {
    bool Sounds=true,Music=false,Prefer3DPBGameData=true,LinearFiltering=false;
    int Players=1,Resolution=0,SoundVolume=128,MusicVolume=0,UpdatesPerSecond=120;
};
class options {
public:
    static optionsStruct Options;
    static std::vector<GameBindings> MapGameInput(GameInput);
    static bool WaitingForInput(){return false;}
    static void InputDown(GameInput){}
};
