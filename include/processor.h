#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stddef.h>
#include "dom.h"

dom_node* process_response(const char *raw_data, size_t length, int make_temp);

#endif
