#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processor.h"
#include "dom.h"

dom_node* process_response(const char *raw_data, size_t length, int make_temp) {
    const char *body_start = strstr(raw_data, "\r\n\r\n");

    if (body_start != NULL) {
        body_start += 4;

        if (make_temp) {
            size_t header_len = body_start - raw_data;
            size_t body_len = length - header_len;
            FILE *file = fopen("temp_page.html", "w");
            if (file != NULL) {
                fwrite(body_start, 1, body_len, file);
                fclose(file);
                printf("saved pure html to temp_page.html\n");
            }
        }

        printf("parsing html tree...\n");
        return parse_html(body_start);
    } else {
        printf("could not find http headers\n");
        return NULL;
    }
}
