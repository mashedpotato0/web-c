#ifndef JS_H
#define JS_H

#include "dom.h"

int init_js();
void run_js(dom_node *root, const char *base_url);
void cleanup_js();

#endif

