/* Native compatibility types used by the original engine. */
#pragma once
#include <stdint.h>
typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef uint8_t Uint8;
typedef uint32_t ImU32;
struct SDL_Texture;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Rect { int x,y,w,h; };
struct SDL_FRect { float x,y,w,h; };
struct Mix_Music;
struct Mix_Chunk {
    unsigned char *samples;
    unsigned frames,rate;
    unsigned short channels,bits;
};
#define MIX_MAX_VOLUME 128
#define SDL_MESSAGEBOX_ERROR 0x10u
#define SDL_MESSAGEBOX_WARNING 0x20u
#define SDLK_F12 0x40000045
#define IM_COL32(R,G,B,A) ((unsigned)(R)|((unsigned)(G)<<8)|((unsigned)(B)<<16)|((unsigned)(A)<<24))
#define IM_FREE free
#define IM_ALLOC malloc
inline const char *SDL_GetHint(const char *) { return nullptr; }
inline int SDL_SetHint(const char *,const char *) { return 1; }
int SDL_ShowSimpleMessageBox(Uint32,const char*,const char*,SDL_Window*);
