#ifndef RENDERER_H
#define RENDERER_H

#include "dom.h"

int init_renderer();
void render_tree(dom_node *root, const char *url_text);
void cleanup_renderer();

#endif
