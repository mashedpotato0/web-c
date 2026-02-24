#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "renderer.h"
#include "css.h"
#include "fetcher.h"
#include "interactive_ui.h"

#define WRAP_NEWLINE(c, h_val) \
do { \
    (c)->y += (c)->line_h + 4; \
    (c)->line_h = (h_val); \
    int new_left = (c)->base_left; \
    if ((c)->y >= (c)->float_l_y && (c)->y < (c)->float_l_bottom) { \
        if (new_left < (c)->float_l_right) new_left = (c)->float_l_right; \
    } \
    (c)->left_edge = new_left; \
    (c)->x = (c)->left_edge; \
} while(0)

static SDL_Renderer *sdl_renderer = NULL;

static TTF_Font *fonts[7] = {NULL};
static int font_sizes[7] = {12, 14, 16, 20, 24, 28, 32};

typedef struct {
    int x;
    int y;
    int line_h;
    int left_edge;
    int base_left;
    int max_w;
    int max_h;
    int scroll_y;
    int float_r_x;
    int float_r_y;
    int float_r_bottom;
    int float_l_right;
    int float_l_y;
    int float_l_bottom;
    dom_node *focused;
    int is_dry_run;
} render_ctx;

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

TTF_Font* get_ui_font() {
    return fonts[2] ? fonts[2] : fonts[0];
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

static void expand_rect(dom_node *node, int ex, int ey, int ew, int eh) {
    dom_node *p = node;
    while (p && p->type == NODE_ELEMENT) {
        if (p->layout.w == 0 || p->layout.h == 0) {
            p->layout.x = ex; p->layout.y = ey; p->layout.w = ew; p->layout.h = eh;
        } else {
            int right = (ex + ew > p->layout.x + p->layout.w) ? (ex + ew) : (p->layout.x + p->layout.w);
            int bottom = (ey + eh > p->layout.y + p->layout.h) ? (ey + eh) : (p->layout.y + p->layout.h);
            if (ex < p->layout.x) p->layout.x = ex;
            if (ey < p->layout.y) p->layout.y = ey;
            p->layout.w = right - p->layout.x;
            p->layout.h = bottom - p->layout.y;
        }
        p = p->parent;
    }
}

static int get_css_len(dom_node *node, const char *prop, int def, int parent_ref) {
    const char *val = get_style(node, prop);
    if (!val) return def;
    int num = atoi(val);
    if (strstr(val, "px")) return num;
    if (strstr(val, "em") || strstr(val, "rem")) return num * 16;
    if (strstr(val, "%")) return (num * parent_ref) / 100;
    return num;
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
            char *slash = strchr(base_url + 8, '/');
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

                char *slash = strchr(url_start, '/');
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

static void draw_text(SDL_Renderer *rend, TTF_Font *font, const char *text, SDL_Color color, render_ctx *ctx, dom_node *parent) {
    char word[1024]; int i = 0, j = 0; int space_w = 0, space_h = 0;
    if (font) TTF_SizeUTF8(font, " ", &space_w, &space_h);
    if (ctx->line_h < space_h) ctx->line_h = space_h;

    while (text[i]) {
        int right_bound = ctx->max_w - 20;
        if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom && ctx->left_edge < ctx->float_r_x) {
            right_bound = ctx->float_r_x - 20;
        }

        int has_space = 0;
        while (text[i] && isspace((unsigned char)text[i])) { has_space = 1; i++; }
        if (ctx->x > ctx->left_edge && has_space) {
            ctx->x += space_w;
            if (ctx->x > right_bound) {
                WRAP_NEWLINE(ctx, space_h);
                if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom && ctx->left_edge < ctx->float_r_x) right_bound = ctx->float_r_x - 20; else right_bound = ctx->max_w - 20;
            }
        }
        if (!text[i]) break;
        j = 0; while (text[i] && !isspace((unsigned char)text[i]) && j < 1023) word[j++] = text[i++]; word[j] = '\0';
        int w = 0, h = 0;
        if (font) TTF_SizeUTF8(font, word, &w, &h);

        if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom && ctx->left_edge < ctx->float_r_x) right_bound = ctx->float_r_x - 20; else right_bound = ctx->max_w - 20;

        if (ctx->x + w > right_bound) WRAP_NEWLINE(ctx, h); else if (h > ctx->line_h) ctx->line_h = h;

        if (font) {
            if (!ctx->is_dry_run) {
                int draw_y = ctx->y - ctx->scroll_y;
                if (draw_y + h > 0 && draw_y < ctx->max_h) {
                    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, word, color);
                    if (surf) {
                        SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
                        SDL_Rect dest = {ctx->x, draw_y, w, h};
                        SDL_RenderCopy(rend, tex, NULL, &dest);
                        SDL_DestroyTexture(tex); SDL_FreeSurface(surf);
                    }
                }
            }
            if (ctx->is_dry_run && parent) expand_rect(parent, ctx->x, ctx->y, w, h);
        }
        ctx->x += w;
    }
}

static void draw_node(dom_node *node, render_ctx *ctx) {
    if (!node) return;

    if (node->type == NODE_ELEMENT) {
        if (node->tag && (strcasecmp(node->tag, "head") == 0 ||
            strcasecmp(node->tag, "style") == 0 ||
            strcasecmp(node->tag, "script") == 0 ||
            strcasecmp(node->tag, "title") == 0 ||
            strcasecmp(node->tag, "meta") == 0 ||
            strcasecmp(node->tag, "link") == 0)) {
            return;
            }

            const char *display = get_style(node, "display");
        if (display && strstr(display, "none")) return;
        const char *pos = get_style(node, "position");
        if (pos && (strstr(pos, "absolute") || strstr(pos, "fixed"))) return;

        if (!ctx->is_dry_run && node->layout.w > 0 && node->layout.h > 0) {
            SDL_Rect r = { node->layout.x, node->layout.y - ctx->scroll_y, node->layout.w, node->layout.h };
            if (r.y + r.h > 0 && r.y < ctx->max_h) {
                const char *bg = get_style(node, "background-color");
                if (bg && !strstr(bg, "transparent") && !strstr(bg, "none")) {
                    SDL_Color col = parse_css_color(bg, (SDL_Color){0,0,0,0});
                    if (col.a > 0) {
                        SDL_SetRenderDrawColor(sdl_renderer, col.r, col.g, col.b, col.a);
                        SDL_RenderFillRect(sdl_renderer, &r);
                    }
                }
                const char *border = get_style(node, "border");
                if (border && !strstr(border, "none") && !strstr(border, "0px")) {
                    SDL_SetRenderDrawColor(sdl_renderer, 200, 200, 200, 255);
                    SDL_RenderDrawRect(sdl_renderer, &r);
                }
            }
        }
    }

    int mt = get_css_len(node, "margin-top", 0, ctx->max_w);
    int mb = get_css_len(node, "margin-bottom", 0, ctx->max_w);
    int ml = get_css_len(node, "margin-left", 0, ctx->max_w);
    int mr = get_css_len(node, "margin-right", 0, ctx->max_w);
    int pt = get_css_len(node, "padding-top", 0, ctx->max_w);
    int pb = get_css_len(node, "padding-bottom", 0, ctx->max_w);
    int pl = get_css_len(node, "padding-left", 0, ctx->max_w);

    const char *flt = get_style(node, "float");
    int is_float_r = (flt && strstr(flt, "right") != NULL);
    int is_float_l = (flt && strstr(flt, "left") != NULL);

    int force_block = 0;
    if (node->type == NODE_ELEMENT) {
        const char *display = get_style(node, "display");
        if (display && (strstr(display, "flex") || strstr(display, "grid") || strstr(display, "block"))) force_block = 1;
    }
    int is_blk = (node->type == NODE_ELEMENT && (is_block(node->tag) || force_block));

    if (is_blk || is_float_r || is_float_l) {
        if (ctx->x > ctx->left_edge) { ctx->x = ctx->left_edge; ctx->y += ctx->line_h; ctx->line_h = 0; }
        ctx->y += mt + pt;
    }

    int pre_float_y = ctx->y;
    int current_left = ctx->base_left + ml + pl;

    if (node->type == NODE_ELEMENT && node->tag) {
        if (strcasecmp(node->tag, "ul") == 0 || strcasecmp(node->tag, "ol") == 0 || strcasecmp(node->tag, "blockquote") == 0) current_left += 40;
    }

    int old_base_left = ctx->base_left;
    int old_left = ctx->left_edge;

    if (is_float_r) {
        if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom) ctx->y = ctx->float_r_bottom + 10;
        ctx->float_r_y = ctx->y;
        int w = get_css_len(node, "width", 300, ctx->max_w); if (w == 0) w = 300;
        ctx->float_r_x = ctx->max_w - w - mr;
        ctx->base_left = ctx->float_r_x + pl; ctx->left_edge = ctx->base_left; ctx->x = ctx->base_left;
    } else if (is_float_l) {
        if (ctx->y >= ctx->float_l_y && ctx->y < ctx->float_l_bottom) ctx->y = ctx->float_l_bottom + 10;
        ctx->float_l_y = ctx->y;
        int w = get_css_len(node, "width", 200, ctx->max_w); if (w == 0) w = 200;
        int nx = current_left + w + mr + 20;
        if (nx > ctx->float_l_right) ctx->float_l_right = nx;
        ctx->base_left = current_left; ctx->left_edge = current_left; ctx->x = current_left;
    } else {
        ctx->base_left = current_left;
        int active_left = current_left;
        if (ctx->y >= ctx->float_l_y && ctx->y < ctx->float_l_bottom) {
            if (active_left < ctx->float_l_right) active_left = ctx->float_l_right;
        }
        ctx->left_edge = active_left;
        if (ctx->x < active_left) ctx->x = active_left;
    }

    if (node->type == NODE_ELEMENT && node->tag) {
        int draw_y = ctx->y - ctx->scroll_y;
        if (ctx->is_dry_run || (draw_y > -500 && draw_y < ctx->max_h + 500)) {
            if (strcasecmp(node->tag, "input") == 0 || strcasecmp(node->tag, "button") == 0) {
                int is_btn = (strcasecmp(node->tag, "button") == 0 || (get_attribute(node, "type") && strcasecmp(get_attribute(node, "type"), "submit") == 0));
                const char *label = get_attribute(node, "value");
                if (!label) label = get_attribute(node, "placeholder");
                if (!label) label = is_btn ? " Submit" : " ";

                int text_w = 0, text_h = 0;
                TTF_Font *btn_f = fonts[2] ? fonts[2] : fonts[0];
                if (btn_f && label[0] != '\0') TTF_SizeUTF8(btn_f, label, &text_w, &text_h);
                int box_w = text_w + 20 < (is_btn ? 80 : 150) ? (is_btn ? 80 : 150) : text_w + 20;

                int right_bound = ctx->max_w - 20;
                if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom && ctx->left_edge < ctx->float_r_x) right_bound = ctx->float_r_x - 20;

                if (ctx->x + box_w > right_bound) {
                    WRAP_NEWLINE(ctx, 0);
                    draw_y = ctx->y - ctx->scroll_y;
                }

                if (ctx->is_dry_run) {
                    expand_rect(node, ctx->x, ctx->y, box_w, 28);
                } else {
                    SDL_Rect box = { ctx->x, draw_y, box_w, 28 };
                    SDL_SetRenderDrawColor(sdl_renderer, is_btn ? 240 : 255, is_btn ? 240 : 255, is_btn ? 240 : 255, 255);
                    SDL_RenderFillRect(sdl_renderer, &box);

                    if (btn_f && label[0] != '\0') {
                        SDL_Color col = {50, 50, 50, 255};
                        SDL_Surface *surf = TTF_RenderUTF8_Blended(btn_f, label, col);
                        if (surf) {
                            SDL_Texture *tex = SDL_CreateTextureFromSurface(sdl_renderer, surf);
                            SDL_Rect d = { ctx->x + 10, draw_y + 5, surf->w, surf->h };
                            SDL_RenderCopy(sdl_renderer, tex, NULL, &d);
                            SDL_DestroyTexture(tex);
                            SDL_FreeSurface(surf);
                        }
                    }
                }
                ctx->x += box_w + 10;
                if (ctx->line_h < 28) ctx->line_h = 28;
            } else if (strcasecmp(node->tag, "img") == 0) {
                int w = 50, h = 30;
                if (node->texture) { w = node->img_w; h = node->img_h; }

                int right_bound = ctx->max_w - 20;
                if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom && ctx->left_edge < ctx->float_r_x) right_bound = ctx->float_r_x - 20;

                int max_avail = right_bound - ctx->x;
                if (max_avail < 10) max_avail = 10;
                if (w > max_avail) {
                    h = h * max_avail / w;
                    w = max_avail;
                }

                if (ctx->x + w > right_bound) {
                    WRAP_NEWLINE(ctx, 0);
                    draw_y = ctx->y - ctx->scroll_y;
                }

                if (ctx->is_dry_run) {
                    expand_rect(node, ctx->x, ctx->y, w, h);
                } else {
                    if (node->texture) {
                        SDL_Rect dest = { ctx->x, draw_y, w, h };
                        SDL_RenderCopy(sdl_renderer, (SDL_Texture*)node->texture, NULL, &dest);
                    } else {
                        SDL_Rect box = { ctx->x, draw_y, w, h };
                        SDL_SetRenderDrawColor(sdl_renderer, 240, 240, 240, 255);
                        SDL_RenderFillRect(sdl_renderer, &box);
                    }
                }
                ctx->x += w + 10;
                if (ctx->line_h < h) ctx->line_h = h;
            } else if (strcasecmp(node->tag, "video") == 0 || strcasecmp(node->tag, "iframe") == 0) {
                // stub out videos and iframes with a player block
                int w = get_css_len(node, "width", 640, ctx->max_w);
                int h = get_css_len(node, "height", 360, ctx->max_w);

                int right_bound = ctx->max_w - 20;
                if (ctx->y >= ctx->float_r_y && ctx->y < ctx->float_r_bottom && ctx->left_edge < ctx->float_r_x) right_bound = ctx->float_r_x - 20;

                int max_avail = right_bound - ctx->x;
                if (max_avail < 10) max_avail = 10;
                if (w > max_avail) {
                    h = h * max_avail / w;
                    w = max_avail;
                }

                if (ctx->x + w > right_bound) {
                    WRAP_NEWLINE(ctx, 0);
                    draw_y = ctx->y - ctx->scroll_y;
                }

                if (ctx->is_dry_run) {
                    expand_rect(node, ctx->x, ctx->y, w, h);
                } else {
                    SDL_Rect box = { ctx->x, draw_y, w, h };
                    SDL_SetRenderDrawColor(sdl_renderer, 30, 30, 30, 255);
                    SDL_RenderFillRect(sdl_renderer, &box);

                    SDL_Rect play_btn = { ctx->x + w/2 - 25, draw_y + h/2 - 15, 50, 30 };
                    SDL_SetRenderDrawColor(sdl_renderer, 200, 0, 0, 255);
                    SDL_RenderFillRect(sdl_renderer, &play_btn);
                }
                ctx->x += w + 10;
                if (ctx->line_h < h) ctx->line_h = h;
            }
        }
    }

    if (node->type == NODE_TEXT && node->text) {
        int font_idx = 2; int is_bold = 0, is_italic = 0; SDL_Color current_color = {30, 30, 30, 255};
        dom_node *p = node->parent;
        while (p != NULL && p->tag != NULL) {
            if (strcasecmp(p->tag, "h1") == 0) { font_idx = 6; is_bold = 1; }
            else if (strcasecmp(p->tag, "h2") == 0) { font_idx = 5; is_bold = 1; }
            else if (strcasecmp(p->tag, "h3") == 0) { font_idx = 4; is_bold = 1; }
            else if (strcasecmp(p->tag, "h4") == 0) { font_idx = 3; is_bold = 1; }
            if (strcasecmp(p->tag, "b") == 0 || strcasecmp(p->tag, "strong") == 0 || strcasecmp(p->tag, "th") == 0) is_bold = 1;
            if (strcasecmp(p->tag, "i") == 0 || strcasecmp(p->tag, "em") == 0) is_italic = 1;
            if (strcasecmp(p->tag, "a") == 0) current_color = (SDL_Color){25, 100, 210, 255};
            const char *color_str = get_style(p, "color");
            if (color_str) { current_color = parse_css_color(color_str, current_color); break; }
            p = p->parent;
        }
        TTF_Font *current_font = fonts[font_idx] ? fonts[font_idx] : fonts[2];
        int style = TTF_STYLE_NORMAL;
        if (is_bold) style |= TTF_STYLE_BOLD;
        if (is_italic) style |= TTF_STYLE_ITALIC;
        if (current_font) TTF_SetFontStyle(current_font, style);
        draw_text(sdl_renderer, current_font, node->text, current_color, ctx, node->parent);
        if (current_font) TTF_SetFontStyle(current_font, TTF_STYLE_NORMAL);
    }

    for (int i = 0; i < node->child_count; i++) draw_node(node->children[i], ctx);

    if (is_blk || is_float_r || is_float_l) {
        ctx->x = old_left;

        ctx->y += ctx->line_h;
        int block_bottom = ctx->y + pb + mb;

        if (is_float_r) {
            ctx->float_r_bottom = block_bottom;
            ctx->float_r_y = pre_float_y;
        }

        if (is_float_l) {
            ctx->float_l_bottom = block_bottom;
            ctx->float_l_y = pre_float_y;
        }

        ctx->line_h = 0;
        ctx->y = block_bottom;

        ctx->left_edge = old_left;
        ctx->base_left = old_base_left;
    }
    if (ctx->y >= ctx->float_r_bottom) {
        ctx->float_r_y = 0;
        ctx->float_r_bottom = 0;
    }

    if (ctx->y >= ctx->float_l_bottom) {
        ctx->float_l_y = 0;
        ctx->float_l_bottom = 0;
        ctx->float_l_right = ctx->base_left;
    }
}

static void reset_layouts(dom_node *node) {
    if (!node) return;
    node->layout.x = 0; node->layout.y = 0; node->layout.w = 0; node->layout.h = 0;
    for (int i = 0; i < node->child_count; i++) reset_layouts(node->children[i]);
}

void render_tree(dom_node *root, int scroll_y, dom_node *focused_node, SDL_Rect viewport) {
    static dom_node *last_root = NULL;
    if (root && root != last_root) { ui_init_document(root); last_root = root; }

    SDL_RenderSetViewport(sdl_renderer, &viewport);

    SDL_SetRenderDrawColor(sdl_renderer, 250, 250, 250, 255);
    SDL_Rect bg_rect = {0, 0, viewport.w, viewport.h};
    SDL_RenderFillRect(sdl_renderer, &bg_rect);

    if (root) {
        reset_layouts(root);
        render_ctx ctx = {0};
        ctx.x = 10; ctx.y = 10; ctx.line_h = 0; ctx.left_edge = 10; ctx.base_left = 10;
        ctx.max_w = viewport.w; ctx.max_h = viewport.h;
        ctx.scroll_y = scroll_y; ctx.focused = focused_node;
        ctx.float_r_x = viewport.w; ctx.float_r_y = 0; ctx.float_r_bottom = 0;
        ctx.float_l_right = 10; ctx.float_l_y = 0; ctx.float_l_bottom = 0;

        ctx.is_dry_run = 1; draw_node(root, &ctx);
        int total_height = ctx.y + ctx.line_h;

        ctx.x = 10; ctx.y = 10; ctx.line_h = 0; ctx.left_edge = 10; ctx.base_left = 10;
        ctx.float_r_x = viewport.w; ctx.float_r_y = 0; ctx.float_r_bottom = 0;
        ctx.float_l_right = 10; ctx.float_l_y = 0; ctx.float_l_bottom = 0;
        ctx.is_dry_run = 0; draw_node(root, &ctx);

        if (total_height > viewport.h) {
            float ratio = (float)viewport.h / total_height;
            int sb_h = (int)(viewport.h * ratio); if (sb_h < 20) sb_h = 20;
            int sb_y = (int)(scroll_y * ratio); if (sb_y + sb_h > viewport.h) sb_y = viewport.h - sb_h;
            SDL_Rect sb_rect = {viewport.w - 10, sb_y, 8, sb_h};
            SDL_SetRenderDrawColor(sdl_renderer, 180, 180, 180, 255);
            SDL_RenderFillRect(sdl_renderer, &sb_rect);
        }
    }

    SDL_RenderSetViewport(sdl_renderer, NULL);
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
