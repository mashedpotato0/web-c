#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "renderer.h"
#include "css.h"
#include "fetcher.h"
#include "interactive_ui.h"
#include "clay.h"

static SDL_Renderer *sdl_renderer = NULL;
static TTF_Font *fonts[7] = {NULL};
static int font_sizes[7] = {12, 14, 16, 20, 24, 28, 32};

static Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Clay_Dimensions dims = {0};
    TTF_Font *font = get_font_by_id(config->fontId);
    if (!font) return dims;

    int w = 0, h = 0;
    char buf[4096];
    int len = text.length < 4095 ? text.length : 4095;
    memcpy(buf, text.chars, len);
    buf[len] = '\0';

    TTF_SizeUTF8(font, buf, &w, &h);
    dims.width = w;
    dims.height = h;
    return dims;
}

int init_renderer(SDL_Renderer *rend) {
    if (TTF_Init() == -1) return -1;
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
        printf("sdl_image init failed: %s\n", IMG_GetError());
    }

    sdl_renderer = rend;
    SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < 7; i++) {
        fonts[i] = TTF_OpenFont("font.ttf", font_sizes[i]);
    }

    return 0;
}

TTF_Font* get_ui_font() { return fonts[2] ? fonts[2] : fonts[0]; }
TTF_Font* get_font_by_id(int id) { return (id >= 0 && id < 7) ? fonts[id] : fonts[2]; }

void setup_clay_measure() {
    static int clay_measure_initialized = 0;
    if (!clay_measure_initialized) {
        Clay_SetMeasureTextFunction(measure_text, NULL);
        clay_measure_initialized = 1;
    }
}

static int get_css_len(dom_node *node, const char *prop, int def) {
    const char *val = get_style(node, prop);
    if (!val) return def;
    int num = atoi(val);
    if (strstr(val, "px")) return num;
    if (strstr(val, "em") || strstr(val, "rem")) return num * 16;
    return num;
}

static int is_block(const char *tag) {
    if (!tag) return 0;
    const char *blocks[] = {
        "div", "p", "h1", "h2", "h3", "h4", "h5", "h6", "ul", "ol", "li",
        "br", "hr", "table", "tr", "tbody", "thead", "body", "html", "form",
        "header", "footer", "nav", "section", "main", "aside", "figure",
        "dt", "dd", "dl", "center", "article", "summary", "details", NULL
    };
    for (int i = 0; blocks[i]; i++) {
        if (strcasecmp(tag, blocks[i]) == 0) return 1;
    }
    return 0;
}

void load_images(dom_node *node, const char *base_url, int download_assets) {
    if (!node) return;

    if (node->tag && strcasecmp(node->tag, "img") == 0 && node->src) {
        char target_url[8192] = {0};

        if (strncmp(node->src, "http", 4) == 0) {
            strncpy(target_url, node->src, 8191);
        } else if (strncmp(node->src, "//", 2) == 0) {
            snprintf(target_url, sizeof(target_url), "https:%s", node->src);
        } else if (node->src[0] == '/') {
            char host[8192] = {0};
            const char *slash = strchr(base_url + 8, '/');
            if (slash) strncpy(host, base_url, slash - base_url);
            else strcpy(host, base_url);
            snprintf(target_url, sizeof(target_url), "%s%s", host, node->src);
        } else {
            char base_copy[8192];
            strncpy(base_copy, base_url, 8191);
            char *slash = strrchr(base_copy, '/');
            if (slash && slash > strchr(base_copy, ':') + 2) *(slash+1) = '\0';
            else strcat(base_copy, "/");
            snprintf(target_url, 8192, "%s%s", base_copy, node->src);
        }

        char hostname[8192] = {0};
        char path[8192] = {0};
        const char *port = "80";
        const char *url_start = target_url;

        if (strncmp(url_start, "http://", 7) == 0) url_start += 7;
            else if (strncmp(url_start, "https://", 8) == 0) { url_start += 8; port = "443"; }

                const char *slash = strchr(url_start, '/');
                if (slash) {
                    strncpy(hostname, url_start, slash - url_start);
                    strcpy(path, slash);
                } else {
                    strcpy(hostname, url_start);
                    strcpy(path, "/");
                }

                size_t img_size = 0;
                char *img_data = fetch_html(hostname, port, path, &img_size);

                if (img_data) {
                    char *body = strstr(img_data, "\r\n\r\n");
                    if (body) {
                        body += 4;
                        size_t body_len = img_size - (body - img_data);

                        if (download_assets) {
                            struct stat st = {0};
                            if (stat("temp_assets", &st) == -1) mkdir("temp_assets", 0700);

                            char filepath[1024];
                            const char *filename = strrchr(path, '/');
                            if (!filename || strlen(filename) <= 1) filename = "/img_fallback.png";

                            char clean_filename[256];
                            strncpy(clean_filename, filename, 255);
                            clean_filename[255] = '\0';
                            char *q = strchr(clean_filename, '?');
                            if (q) *q = '\0';

                            snprintf(filepath, sizeof(filepath), "temp_assets%s", clean_filename);
                            FILE *f = fopen(filepath, "wb");
                            if (f) {
                                fwrite(body, 1, body_len, f);
                                fclose(f);
                            }
                        }

                        SDL_RWops *rw = SDL_RWFromMem(body, body_len);
                        node->texture = IMG_LoadTexture_RW(sdl_renderer, rw, 1);
                        if (node->texture) {
                            SDL_QueryTexture((SDL_Texture*)node->texture, NULL, NULL, &node->img_w, &node->img_h);
                        }
                    }
                    free(img_data);
                }
    }

    for (int i = 0; i < node->child_count; i++) {
        load_images(node->children[i], base_url, download_assets);
    }
}

static void draw_node_clay(dom_node *node) {
    if (!node) return;

    if (node->type == NODE_TEXT && node->text && strlen(node->text) > 0) {
        int is_empty = 1;
        for (int i = 0; node->text[i]; i++) {
            if (!isspace((unsigned char)node->text[i])) { is_empty = 0; break; }
        }
        if (is_empty) return;

        int font_idx = 2;
        SDL_Color current_color = {30, 30, 30, 255};
        dom_node *p = node->parent;

        while (p != NULL && p->tag != NULL) {
            if (strcasecmp(p->tag, "h1") == 0) font_idx = 6;
            else if (strcasecmp(p->tag, "h2") == 0) font_idx = 5;
            else if (strcasecmp(p->tag, "h3") == 0) font_idx = 4;
            else if (strcasecmp(p->tag, "h4") == 0) font_idx = 3;
            if (strcasecmp(p->tag, "a") == 0) current_color = (SDL_Color){25, 100, 210, 255};

            const char *color_str = get_style(p, "color");
            if (color_str) { current_color = parse_css_color(color_str, current_color); break; }
            p = p->parent;
        }

        Clay_TextElementConfig txt_cfg = {
            .textColor = {current_color.r, current_color.g, current_color.b, current_color.a},
            .fontId = font_idx,
            .fontSize = font_sizes[font_idx],
            .wrapMode = CLAY_TEXT_WRAP_WORDS
        };
        Clay_String str = { .chars = node->text, .length = (int)strlen(node->text) };
        uint32_t node_idx = (uint32_t)((uintptr_t)node & 0xFFFFFFFF);

        Clay__OpenElementWithId(CLAY_IDI("node", node_idx));
        Clay__OpenTextElement(str, &txt_cfg);
        Clay__CloseElement();
        return;
    }

    if (node->type == NODE_ELEMENT) {
        if (node->tag && (strcasecmp(node->tag, "head") == 0 || strcasecmp(node->tag, "style") == 0 ||
            strcasecmp(node->tag, "script") == 0 || strcasecmp(node->tag, "title") == 0 ||
            strcasecmp(node->tag, "meta") == 0 || strcasecmp(node->tag, "link") == 0)) {
            return;
            }

            const char *display = get_style(node, "display");
        if (display && strstr(display, "none")) return;

        Clay_ElementDeclaration decl = {0};
        uint32_t node_idx = (uint32_t)((uintptr_t)node & 0xFFFFFFFF);

        int mt = get_css_len(node, "margin-top", 0);
        int mb = get_css_len(node, "margin-bottom", 0);
        int ml = get_css_len(node, "margin-left", 0);
        int mr = get_css_len(node, "margin-right", 0);
        int pt = get_css_len(node, "padding-top", 0);
        int pb = get_css_len(node, "padding-bottom", 0);
        int pl = get_css_len(node, "padding-left", 0);
        int pr = get_css_len(node, "padding-right", 0);

        if (node->tag && (strcasecmp(node->tag, "ul") == 0 || strcasecmp(node->tag, "ol") == 0 || strcasecmp(node->tag, "blockquote") == 0)) pl += 40;

        decl.layout.padding = (Clay_Padding){pl + ml, pr + mr, pt + mt, pb + mb};
        decl.layout.childGap = 5;

        if (is_block(node->tag)) {
            decl.layout.sizing.width = CLAY_SIZING_GROW();
            decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        } else {
            decl.layout.sizing.width = CLAY_SIZING_FIT();
            decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        }

        const char *bg = get_style(node, "background-color");
        if (bg && !strstr(bg, "transparent") && !strstr(bg, "none")) {
            SDL_Color col = parse_css_color(bg, (SDL_Color){0,0,0,0});
            decl.backgroundColor = (Clay_Color){col.r, col.g, col.b, col.a};
        }

        if (node->tag && strcasecmp(node->tag, "img") == 0) {
            decl.layout.sizing.width = CLAY_SIZING_FIXED(node->img_w > 0 ? node->img_w : 50);
            decl.layout.sizing.height = CLAY_SIZING_FIXED(node->img_h > 0 ? node->img_h : 50);
            decl.image = (Clay_ImageElementConfig){.imageData = node->texture};
        }

        if (node->tag && (strcasecmp(node->tag, "input") == 0 || strcasecmp(node->tag, "button") == 0)) {
            decl.layout.sizing.width = CLAY_SIZING_FIT();
            decl.layout.sizing.height = CLAY_SIZING_FIXED(28);
            decl.backgroundColor = (Clay_Color){240, 240, 240, 255};
            decl.layout.padding = (Clay_Padding){10, 10, 5, 5};
        }

        Clay__OpenElementWithId(CLAY_IDI("node", node_idx));
        Clay__ConfigureOpenElement(decl);

        if (node->tag && (strcasecmp(node->tag, "input") == 0 || strcasecmp(node->tag, "button") == 0)) {
            int is_btn = (strcasecmp(node->tag, "button") == 0 || (get_attribute(node, "type") && strcasecmp(get_attribute(node, "type"), "submit") == 0));
            const char *label = get_attribute(node, "value");
            if (!label) label = get_attribute(node, "placeholder");
            if (!label) label = is_btn ? " Submit" : " ";

            Clay_TextElementConfig txt_cfg = { .textColor = {50, 50, 50, 255}, .fontId = 2, .fontSize = 16 };
            Clay_String str = { .chars = label, .length = (int)strlen(label) };
            Clay__OpenTextElement(str, &txt_cfg);
        }

        for (int i = 0; i < node->child_count; i++) {
            draw_node_clay(node->children[i]);
        }

        Clay__CloseElement();
    }
}

void update_dom_layouts(dom_node *node) {
    if (!node) return;
    uint32_t node_idx = (uint32_t)((uintptr_t)node & 0xFFFFFFFF);
    Clay_ElementData data = Clay_GetElementData(Clay_GetElementIdWithIndex(CLAY_STRING("node"), node_idx));

    if (data.found) {
        node->layout.x = data.boundingBox.x;
        node->layout.y = data.boundingBox.y;
        node->layout.w = data.boundingBox.width;
        node->layout.h = data.boundingBox.height;
    }
    for (int i = 0; i < node->child_count; i++) update_dom_layouts(node->children[i]);
}

void render_tree(dom_node *root, int scroll_y, dom_node *focused_node, SDL_Rect viewport) {
    static dom_node *last_root = NULL;
    if (root && root != last_root) { ui_init_document(root); last_root = root; }

    Clay_ElementDeclaration decl = {0};
    decl.layout.sizing.width = CLAY_SIZING_GROW();
    decl.layout.sizing.height = CLAY_SIZING_FIT();
    decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
    decl.floating.offset = (Clay_Vector2){0, -scroll_y};

    Clay__OpenElementWithId(CLAY_ID("page_content"));
    Clay__ConfigureOpenElement(decl);
    draw_node_clay(root);
    Clay__CloseElement();
}

void free_textures(dom_node *node) {
    if (!node) return;
    if (node->texture) { SDL_DestroyTexture((SDL_Texture*)node->texture); node->texture = NULL; }
    for (int i = 0; i < node->child_count; i++) free_textures(node->children[i]);
}

void cleanup_renderer() {
    for (int i = 0; i < 7; i++) if (fonts[i]) TTF_CloseFont(fonts[i]);
    IMG_Quit(); TTF_Quit();
}
