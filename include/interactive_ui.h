#ifndef INTERACTIVE_UI_H
#define INTERACTIVE_UI_H

#include "dom.h"

void ui_init_document(dom_node *root);
int ui_handle_click(dom_node *root, int mx, int my, int scroll_y);

#endif

