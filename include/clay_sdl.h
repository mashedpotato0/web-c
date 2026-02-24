#ifndef CLAY_SDL_H
#define CLAY_SDL_H

#include <SDL2/SDL.h>
#include "clay.h"

void clay_sdl_render(SDL_Renderer *renderer, Clay_RenderCommandArray commands);

#endif
