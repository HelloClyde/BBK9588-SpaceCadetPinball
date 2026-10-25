#pragma once
enum class GameBindings;
class winmain {
public:
    static bool single_step,LaunchBallEnabled,HighScoresEnabled,DemoActive;
    static SDL_Window* MainWindow;
    static SDL_Renderer* Renderer;
    static int MainMenuHeight;
    static void memalloc_failure();
    static void new_game();
    static void pause(bool toggle=true);
    static void end_pause();
    static void HandleGameBinding(GameBindings,bool);
};
