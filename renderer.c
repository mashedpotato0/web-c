#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "renderer.h"

static SDL_Window *window = NULL;
static SDL_Renderer *sdl_renderer = NULL;
static TTF_Font *font_normal = NULL;
static TTF_Font *font_h1 = NULL;

int init_renderer() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("sdl init failed: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() == -1) {
        printf("ttf init failed: %s\n", TTF_GetError());
        return -1;
    }

    window = SDL_CreateWindow("c browser", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("window creation failed: %s\n", SDL_GetError());
        return -1;
    }

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl_renderer) {
        printf("renderer creation failed: %s\n", SDL_GetError());
        return -1;
    }

    font_normal = TTF_OpenFont("font.ttf", 16);
    font_h1 = TTF_OpenFont("font.ttf", 32);

    if (!font_normal || !font_h1) {
        printf("failed to load fonts\n");
    }

    return 0;
}

static void draw_node(dom_node *node, int *current_y) {
    if (!node) return;

    if (node->type == NODE_ELEMENT && node->tag) {
        if (strcmp(node->tag, "head") == 0 ||
            strcmp(node->tag, "style") == 0 ||
            strcmp(node->tag, "script") == 0 ||
            strcmp(node->tag, "title") == 0) {
            return;
            }
    }

    if (node->type == NODE_TEXT && node->text) {
        int is_empty = 1;
        for (int i = 0; node->text[i]; i++) {
            if (!isspace((unsigned char)node->text[i])) {
                is_empty = 0;
                break;
            }
        }

        if (!is_empty) {
            TTF_Font *current_font = font_normal;
            SDL_Color current_color = {0, 0, 0, 255};

            dom_node *p = node->parent;
            while (p != NULL && p->tag != NULL) {
                if (strcmp(p->tag, "h1") == 0) {
                    current_font = font_h1;
                } else if (strcmp(p->tag, "a") == 0) {
                    current_color = (SDL_Color){0, 0, 255, 255};
                }
                p = p->parent;
            }

            if (current_font) {
                SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(current_font, node->text, current_color, 780);
                if (surface) {
                    SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
                    SDL_Rect dest = { 10, *current_y, surface->w, surface->h };

                    if (node->parent) {
                        node->parent->layout.x = dest.x;
                        node->parent->layout.y = dest.y;
                        node->parent->layout.w = dest.w;
                        node->parent->layout.h = dest.h;
                    }

                    SDL_RenderCopy(sdl_renderer, texture, NULL, &dest);
                    *current_y += surface->h + 10;

                    SDL_DestroyTexture(texture);
                    SDL_FreeSurface(surface);
                }
            }
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        draw_node(node->children[i], current_y);
    }
}

void render_tree(dom_node *root, const char *url_text) {
    SDL_SetRenderDrawColor(sdl_renderer, 255, 255, 255, 255);
    SDL_RenderClear(sdl_renderer);

    SDL_Rect url_rect = {0, 0, 800, 40};
    SDL_SetRenderDrawColor(sdl_renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(sdl_renderer, &url_rect);

    if (font_normal && url_text[0] != '\0') {
        SDL_Color text_color = {0, 0, 0, 255};
        SDL_Surface *surface = TTF_RenderText_Blended(font_normal, url_text, text_color);
        if (surface) {
            SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
            SDL_Rect dest = { 10, 10, surface->w, surface->h };
            SDL_RenderCopy(sdl_renderer, texture, NULL, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    int start_y = 50;
    draw_node(root, &start_y);

    SDL_RenderPresent(sdl_renderer);
}

void cleanup_renderer() {
    if (font_normal) TTF_CloseFont(font_normal);
    if (font_h1) TTF_CloseFont(font_h1);
    if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}
