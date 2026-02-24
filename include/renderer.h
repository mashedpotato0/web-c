#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "dom.h"

int init_renderer(SDL_Renderer *rend);
void load_images(dom_node *node, const char *base_url, int download_assets);
void render_tree(dom_node *root, int scroll_y, dom_node *focused_node, SDL_Rect viewport);
void update_dom_layouts(dom_node *node);

void free_textures(dom_node *node);
void cleanup_renderer();
TTF_Font* get_ui_font();
TTF_Font* get_font_by_id(int id);
void setup_clay_measure();

#endif
