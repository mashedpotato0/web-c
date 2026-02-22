#ifndef FETCHER_H
#define FETCHER_H

#include <stddef.h>

char* fetch_html(const char *hostname, const char *port, const char *path, size_t *out_size);

#endif
