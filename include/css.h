#ifndef CSS_H
#define CSS_H

#include <SDL2/SDL.h>
#include "dom.h"

void process_css(dom_node *root, const char *base_url, int download_assets);
SDL_Color parse_css_color(const char *color_str, SDL_Color def);

#endif
