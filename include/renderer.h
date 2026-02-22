#ifndef RENDERER_H
#define RENDERER_H

#include "dom.h"

int init_renderer();
void load_images(dom_node *node, const char *base_url, int download_assets);
void render_tree(dom_node *root, const char *url_text, int scroll_y, dom_node *focused_node);
void free_textures(dom_node *node);
void cleanup_renderer();

#endif
