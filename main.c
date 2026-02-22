#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "fetcher.h"
#include "processor.h"
#include "renderer.h"

void load_url(const char *url, dom_node **tree, int make_temp);

int check_click(dom_node *node, int mx, int my, char *url_buffer, dom_node **tree, int make_temp) {
    if (!node) return 0;

    if (node->tag && strcmp(node->tag, "a") == 0 && node->href) {
        if (mx >= node->layout.x && mx <= node->layout.x + node->layout.w &&
            my >= node->layout.y && my <= node->layout.y + node->layout.h) {

            printf("clicked link: %s\n", node->href);

        char target_url[512];
        if (strncmp(node->href, "http://", 7) == 0) {
            strcpy(target_url, node->href + 7);
        } else {
            strcpy(target_url, node->href);
        }

        strncpy(url_buffer, target_url, 255);
        load_url(target_url, tree, make_temp);
        return 1;
            }
    }

    for (int i = 0; i < node->child_count; i++) {
        if (check_click(node->children[i], mx, my, url_buffer, tree, make_temp)) {
            return 1;
        }
    }

    return 0;
}

void load_url(const char *url, dom_node **tree, int make_temp) {
    char hostname[256] = {0};
    char path[256] = {0};
    const char *port = "80";

    char *slash = strchr(url, '/');
    if (slash) {
        strncpy(hostname, url, slash - url);
        strcpy(path, slash);
    } else {
        strcpy(hostname, url);
        strcpy(path, "/");
    }

    printf("fetching %s%s...\n", hostname, path);
    size_t data_size = 0;
    char *raw_data = fetch_html(hostname, port, path, &data_size);

    if (*tree) {
        free_tree(*tree);
        *tree = NULL;
    }

    if (raw_data != NULL) {
        *tree = process_response(raw_data, data_size, make_temp);
        free(raw_data);
    } else {
        printf("failed to fetch website data\n");
    }
}

int main() {
    int make_temp = 1;
    char url_buffer[256] = "example.com";
    dom_node *tree = NULL;

    printf("starting browser process...\n");

    if (init_renderer() != 0) {
        return 1;
    }

    load_url(url_buffer, &tree, make_temp);

    int running = 1;
    SDL_Event event;
    SDL_StartTextInput();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    check_click(tree, event.button.x, event.button.y, url_buffer, &tree, make_temp);
                }
            } else if (event.type == SDL_TEXTINPUT) {
                if (strlen(url_buffer) + strlen(event.text.text) < sizeof(url_buffer) - 1) {
                    strcat(url_buffer, event.text.text);
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && strlen(url_buffer) > 0) {
                    url_buffer[strlen(url_buffer) - 1] = '\0';
                } else if (event.key.keysym.sym == SDLK_RETURN) {
                    load_url(url_buffer, &tree, make_temp);
                }
            }
        }

        render_tree(tree, url_buffer);
        SDL_Delay(16);
    }

    printf("cleaning up and exiting...\n");
    if (tree) free_tree(tree);
    SDL_StopTextInput();
    cleanup_renderer();

    return 0;
}
