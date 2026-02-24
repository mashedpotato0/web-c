#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "fetcher.h"
#include "processor.h"
#include "renderer.h"
#include "css.h"
#include "ui.h"
#include "interactive_ui.h"
#include "clay.h"
#include "clay_sdl.h"

#ifndef MAX_URL
#define MAX_URL MAX_URL_LEN
#endif

void load_url(char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets);

dom_node* find_node_by_tag(dom_node *node, const char *tag) {
    if (!node || !tag) return NULL;
    if (node->tag && strcasecmp(node->tag, tag) == 0) return node;
    for (int i = 0; i < node->child_count; i++) {
        dom_node *res = find_node_by_tag(node->children[i], tag);
        if (res) return res;
    }
    return NULL;
}

void update_tab_title(browser_tab *tab) {
    dom_node *root = (dom_node*)tab->tree;
    if (!root) return;
    dom_node *title_node = find_node_by_tag(root, "title");
    if (title_node && title_node->child_count > 0 && title_node->children[0]->type == NODE_TEXT) {
        strncpy(tab->title, title_node->children[0]->text, 127);
        tab->title[127] = '\0';
    } else {
        strncpy(tab->title, "web page", 127);
    }
}

dom_node* find_text_input(dom_node *node) {
    if (!node) return NULL;
    if (node->tag && strcasecmp(node->tag, "input") == 0) {
        const char *type = get_attribute(node, "type");
        if (!type || strcasecmp(type, "text") == 0 || strcasecmp(type, "search") == 0) return node;
    }
    for (int i = 0; i < node->child_count; i++) {
        dom_node *res = find_text_input(node->children[i]);
        if (res) return res;
    }
    return NULL;
}

dom_node* find_element_by_id(dom_node *node, const char *id) {
    if (!node || !id) return NULL;
    const char *node_id = get_attribute(node, "id");
    if (node_id && strcmp(node_id, id) == 0) return node;
    for (int i = 0; i < node->child_count; i++) {
        dom_node *res = find_element_by_id(node->children[i], id);
        if (res) return res;
    }
    return NULL;
}

int submit_form(dom_node *node, char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets) {
    dom_node *form = node;
    while (form && (!form->tag || strcasecmp(form->tag, "form") != 0)) {
        form = form->parent;
    }
    if (!form) return 0;

    const char *action = get_attribute(form, "action");
    if (!action || action[0] == '\0') action = "/";

    dom_node *input = find_text_input(form);
    const char *val = input ? get_attribute(input, "value") : "";
    if (!val) val = "";

    char *target_url = calloc(1, MAX_URL * 3);
    char *current_host = calloc(1, MAX_URL);
    const char *start = url_buffer;

    if (strncmp(start, "http://", 7) == 0) start += 7;
        else if (strncmp(start, "https://", 8) == 0) start += 8;

            const char *p_delim = strpbrk(start, "/?");
    if (p_delim) {
        size_t hlen = p_delim - url_buffer;
        if (hlen >= MAX_URL) hlen = MAX_URL - 1;
        strncpy(current_host, url_buffer, hlen);
    } else {
        strncpy(current_host, url_buffer, MAX_URL - 1);
    }

    char *encoded_val = calloc(1, MAX_URL);
    int j = 0;
    for (int i = 0; val[i] && j < MAX_URL - 10; i++) {
        if (val[i] == ' ') encoded_val[j++] = '+';
        else encoded_val[j++] = val[i];
    }

    const char *input_name = input ? get_attribute(input, "name") : "q";
    if (!input_name) input_name = "q";

    if (strncmp(action, "http", 4) == 0) {
        snprintf(target_url, MAX_URL * 3, "%s?%s=%s", action, input_name, encoded_val);
    } else if (action[0] == '/') {
        snprintf(target_url, MAX_URL * 3, "%s%s?%s=%s", current_host, action, input_name, encoded_val);
    } else {
        snprintf(target_url, MAX_URL * 3, "%s/%s?%s=%s", current_host, action, input_name, encoded_val);
    }

    strncpy(url_buffer, target_url, MAX_URL - 1);
    url_buffer[MAX_URL - 1] = '\0';

    free(target_url);
    free(current_host);
    free(encoded_val);

    return 1;
}

int check_click(dom_node *node, int mx, int my, char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets) {
    if (!node) return 0;

    int absolute_y = my;
    int is_inside = (node->layout.w > 0 && node->layout.h > 0 &&
    mx >= node->layout.x && mx <= node->layout.x + node->layout.w &&
    absolute_y >= node->layout.y && absolute_y <= node->layout.y + node->layout.h);

    if (is_inside) {
        if (node->tag && strcasecmp(node->tag, "a") == 0 && node->href) {
            if (node->href[0] == '#') {
                dom_node *target = find_element_by_id(*tree, node->href + 1);
                if (target) *scroll_y = target->layout.y > 40 ? target->layout.y - 40 : 0;
                return 0;
            }

            if (strncmp(node->href, "javascript:", 11) == 0 || strncmp(node->href, "mailto:", 7) == 0) return 0;

            char *target_url = calloc(1, MAX_URL * 3);
            if (strncmp(node->href, "http://", 7) == 0 || strncmp(node->href, "https://", 8) == 0) {
                strncpy(target_url, node->href, (MAX_URL * 3) - 1);
            } else if (strncmp(node->href, "//", 2) == 0) {
                snprintf(target_url, MAX_URL * 3, "https:%s", node->href);
            } else if (node->href[0] == '/') {
                char *current_host = calloc(1, MAX_URL);
                const char *start = url_buffer;
                if (strncmp(start, "http://", 7) == 0) start += 7;
                    else if (strncmp(start, "https://", 8) == 0) start += 8;

                        const char *p_delim = strpbrk(start, "/?");
                if (p_delim) {
                    size_t hlen = p_delim - url_buffer;
                    if (hlen >= MAX_URL) hlen = MAX_URL - 1;
                    strncpy(current_host, url_buffer, hlen);
                } else {
                    strncpy(current_host, url_buffer, MAX_URL - 1);
                }
                snprintf(target_url, MAX_URL * 3, "%s%s", current_host, node->href);
                free(current_host);
            } else {
                char *base_url = calloc(1, MAX_URL);
                strncpy(base_url, url_buffer, MAX_URL - 1);
                char *p_delim = strpbrk(base_url + (strncmp(base_url, "http", 4) == 0 ? 8 : 0), "?");
                if (p_delim) *p_delim = '\0';

                char *last_slash = strrchr(base_url, '/');
                char *colon = strchr(base_url, ':');

                if (last_slash && colon && last_slash > colon + 2) {
                    *(last_slash + 1) = '\0';
                } else if (last_slash && !colon) {
                    *(last_slash + 1) = '\0';
                } else if (!last_slash) {
                    strcat(base_url, "/");
                }

                snprintf(target_url, MAX_URL * 3, "%s%s", base_url, node->href);
                free(base_url);
            }

            strncpy(url_buffer, target_url, MAX_URL - 1);
            url_buffer[MAX_URL - 1] = '\0';
            free(target_url);

            return 1;
        }

        if (node->tag && strcasecmp(node->tag, "input") == 0) {
            const char *type = get_attribute(node, "type");
            if (!type || strcasecmp(type, "text") == 0 || strcasecmp(type, "search") == 0) {
                *focused_node = node;
                return 0;
            }
            if (type && strcasecmp(type, "submit") == 0) {
                return submit_form(node, url_buffer, tree, make_temp, scroll_y, focused_node, download_assets);
            }
        }

        if (node->tag && strcasecmp(node->tag, "button") == 0) {
            return submit_form(node, url_buffer, tree, make_temp, scroll_y, focused_node, download_assets);
        }
    }

    for (int i = 0; i < node->child_count; i++) {
        if (check_click(node->children[i], mx, my, url_buffer, tree, make_temp, scroll_y, focused_node, download_assets)) {
            return 1;
        }
    }

    return 0;
}

void load_url(char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets) {
    if (scroll_y) *scroll_y = 0;
    if (focused_node) *focused_node = NULL;

    int redirect_count = 0;
    while (redirect_count < 5) {
        char hostname[MAX_URL] = {0};
        char path[MAX_URL] = {0};
        const char *port = "80";

        const char *url_start = url_buffer;
        if (strncmp(url_start, "http://", 7) == 0) url_start += 7;
            else if (strncmp(url_start, "https://", 8) == 0) { url_start += 8; port = "443"; }

                const char *p_delim = strpbrk(url_start, "/?");
                if (p_delim) {
                    size_t len = p_delim - url_start;
                    if (len >= MAX_URL) len = MAX_URL - 1;
                    strncpy(hostname, url_start, len);
                    hostname[len] = '\0';

                    if (*p_delim == '?') {
                        path[0] = '/';
                        strncpy(path + 1, p_delim, MAX_URL - 2);
                    } else {
                        strncpy(path, p_delim, MAX_URL - 1);
                    }
                    path[MAX_URL - 1] = '\0';
                } else {
                    strncpy(hostname, url_start, MAX_URL - 1);
                    hostname[MAX_URL - 1] = '\0';
                    strcpy(path, "/");
                }

                size_t data_size = 0;
                char *raw_data = fetch_html(hostname, port, path, &data_size);

                if (raw_data != NULL) {
                    int is_redirect = 0;
                    if (strncmp(raw_data, "HTTP/", 5) == 0) {
                        char *space = strchr(raw_data, ' ');
                        if (space) {
                            int status = atoi(space + 1);
                            if (status >= 300 && status < 400) {
                                char *loc = strstr(raw_data, "\r\nLocation: ");
                                if (!loc) loc = strstr(raw_data, "\r\nlocation: ");
                                if (!loc) loc = strstr(raw_data, "\nLocation: ");
                                if (loc) {
                                    loc = strchr(loc, ':') + 1;
                                    while (*loc == ' ') loc++;
                                    char *end = strstr(loc, "\r\n");
                                    if (!end) end = strchr(loc, '\n');

                                    if (end) {
                                        char new_target[MAX_URL] = {0};
                                        size_t cplen = end - loc;
                                        if (cplen >= MAX_URL) cplen = MAX_URL - 1;
                                        strncpy(new_target, loc, cplen);

                                        free(raw_data);
                                        redirect_count++;
                                        is_redirect = 1;

                                        char *new_url = calloc(1, MAX_URL * 3);
                                        if (new_target[0] == '/') {
                                            snprintf(new_url, MAX_URL * 3, "%s://%s%s", strcmp(port, "443") == 0 ? "https" : "http", hostname, new_target);
                                        } else if (strncmp(new_target, "//", 2) == 0) {
                                            snprintf(new_url, MAX_URL * 3, "https:%s", new_target);
                                        } else {
                                            strncpy(new_url, new_target, (MAX_URL * 3) - 1);
                                        }

                                        strncpy(url_buffer, new_url, MAX_URL - 1);
                                        url_buffer[MAX_URL - 1] = '\0';
                                        free(new_url);
                                    }
                                }
                            }
                        }
                    }

                    if (!is_redirect) {
                        if (*tree) {
                            free_textures(*tree);
                            free_tree(*tree);
                            *tree = NULL;
                        }
                        *tree = process_response(raw_data, data_size, make_temp);
                        if (*tree) {
                            char base_url[MAX_URL + 16] = {0};
                            snprintf(base_url, sizeof(base_url), "%s://%s", strcmp(port, "443") == 0 ? "https" : "http", hostname);
                            process_css(*tree, base_url, download_assets);
                            load_images(*tree, base_url, download_assets);
                        }
                        free(raw_data);
                        break;
                    }
                } else {
                    break;
                }
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window *window = SDL_CreateWindow("c browser", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    SDL_Renderer *rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (init_renderer(rend) != 0) return 1;

    uint64_t clay_memory_size = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, malloc(clay_memory_size));
    Clay_Initialize(arena, (Clay_Dimensions) { 1280, 720 }, (Clay_ErrorHandler) { 0 });

    browser_ui ui;
    init_ui(&ui);

    int running = 1;
    int pending_load = 1;
    SDL_Event event;
    SDL_StartTextInput();
    dom_node *focused_node = NULL;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEMOTION) {
                Clay_SetPointerState((Clay_Vector2){ event.motion.x, event.motion.y }, false);
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                Clay_SetPointerState((Clay_Vector2){ event.button.x, event.button.y }, true);
                if (event.button.y < 80) {
                    int closed_tab_idx = -1;
                    int ui_action = handle_ui_click(&ui, event.button.x, event.button.y, &closed_tab_idx);
                    if (ui_action > 0) {
                        focused_node = NULL;
                        if (ui_action == 2) {
                            if (ui.tabs[closed_tab_idx].tree) {
                                free_textures((dom_node*)ui.tabs[closed_tab_idx].tree);
                                free_tree((dom_node*)ui.tabs[closed_tab_idx].tree);
                            }
                            for (int i = closed_tab_idx; i < ui.tab_count - 1; i++) {
                                ui.tabs[i] = ui.tabs[i + 1];
                            }
                            ui.tab_count--;
                            if (ui.tab_count == 0) {
                                running = 0;
                            } else {
                                if (ui.active_tab >= ui.tab_count) ui.active_tab = ui.tab_count - 1;
                                strncpy(ui.input_buffer, ui.tabs[ui.active_tab].url, MAX_URL - 1);
                            }
                        } else if (ui_action == 3) {
                            pending_load = 1;
                        }
                    }
                } else {
                    ui.is_focused = 0;
                    browser_tab *tab = &ui.tabs[ui.active_tab];
                    int page_my = event.button.y - 80;

                    // ui_handle_click adds scroll_y internally, so subtract it to pass pure viewport Y
                    if (!ui_handle_click((dom_node*)tab->tree, event.button.x, page_my - tab->scroll_y, tab->scroll_y)) {
                        if (check_click((dom_node*)tab->tree, event.button.x, page_my, tab->url, (dom_node**)&tab->tree, 1, &tab->scroll_y, &focused_node, 1)) {
                            strncpy(ui.input_buffer, tab->url, MAX_URL - 1);
                            pending_load = 1;
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                Clay_SetPointerState((Clay_Vector2){ event.button.x, event.button.y }, false);
            } else if (event.type == SDL_MOUSEWHEEL) {
                ui.tabs[ui.active_tab].scroll_y -= event.wheel.y * 30;
                if (ui.tabs[ui.active_tab].scroll_y < 0) ui.tabs[ui.active_tab].scroll_y = 0;
            } else if (event.type == SDL_TEXTINPUT) {
                if (ui.is_focused) {
                    if (strlen(ui.input_buffer) + strlen(event.text.text) < MAX_URL - 1) {
                        strcat(ui.input_buffer, event.text.text);
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && ui.is_focused) {
                    if (strlen(ui.input_buffer) > 0) ui.input_buffer[strlen(ui.input_buffer) - 1] = '\0';
                } else if (event.key.keysym.sym == SDLK_RETURN && ui.is_focused) {
                    char target_url[MAX_URL];
                    ui_format_search(ui.input_buffer, target_url);
                    strncpy(ui.tabs[ui.active_tab].url, target_url, MAX_URL - 1);
                    strncpy(ui.input_buffer, target_url, MAX_URL - 1);
                    pending_load = 1;
                    ui.is_focused = 0;
                } else if (event.key.keysym.sym == SDLK_DOWN && !ui.is_focused) {
                    ui.tabs[ui.active_tab].scroll_y += 30;
                } else if (event.key.keysym.sym == SDLK_UP && !ui.is_focused) {
                    ui.tabs[ui.active_tab].scroll_y -= 30;
                    if (ui.tabs[ui.active_tab].scroll_y < 0) ui.tabs[ui.active_tab].scroll_y = 0;
                }
            }
        }

        ui.is_loading = pending_load;
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        // 1. page layout (isolated)
        Clay_SetLayoutDimensions((Clay_Dimensions) { win_w, win_h - 80 });
        Clay_BeginLayout();
        SDL_Rect viewport = {0, 80, win_w, win_h - 80};
        render_tree((dom_node*)ui.tabs[ui.active_tab].tree, ui.tabs[ui.active_tab].scroll_y, focused_node, viewport);
        Clay_RenderCommandArray page_cmds = Clay_EndLayout();

        // copy page commands to prevent arena overwrite in pass 2
        Clay_RenderCommand *page_cmds_copy = NULL;
        if (page_cmds.length > 0) {
            page_cmds_copy = malloc(page_cmds.length * sizeof(Clay_RenderCommand));
            memcpy(page_cmds_copy, page_cmds.internalArray, page_cmds.length * sizeof(Clay_RenderCommand));
        }
        Clay_RenderCommandArray saved_page_cmds = page_cmds;
        saved_page_cmds.internalArray = page_cmds_copy;

        if (ui.tabs[ui.active_tab].tree) {
            update_dom_layouts((dom_node*)ui.tabs[ui.active_tab].tree);
        }

        // 2. ui layout (isolated)
        Clay_SetLayoutDimensions((Clay_Dimensions) { win_w, 80 });
        Clay_BeginLayout();
        draw_ui(&ui, win_w);
        Clay_RenderCommandArray ui_cmds = Clay_EndLayout();

        // dynamically bind interaction rects to internal component sizes
        Clay_ElementData btn_add = Clay_GetElementData(Clay_GetElementId(CLAY_STRING("add_tab_btn")));
        ui.add_tab_btn = (SDL_Rect){ btn_add.boundingBox.x, btn_add.boundingBox.y, btn_add.boundingBox.width, btn_add.boundingBox.height };

        Clay_ElementData btn_back = Clay_GetElementData(Clay_GetElementId(CLAY_STRING("back_btn")));
        ui.back_btn = (SDL_Rect){ btn_back.boundingBox.x, btn_back.boundingBox.y, btn_back.boundingBox.width, btn_back.boundingBox.height };

        Clay_ElementData btn_fwd = Clay_GetElementData(Clay_GetElementId(CLAY_STRING("fwd_btn")));
        ui.fwd_btn = (SDL_Rect){ btn_fwd.boundingBox.x, btn_fwd.boundingBox.y, btn_fwd.boundingBox.width, btn_fwd.boundingBox.height };

        Clay_ElementData box_url = Clay_GetElementData(Clay_GetElementId(CLAY_STRING("url_box")));
        ui.url_box = (SDL_Rect){ box_url.boundingBox.x, box_url.boundingBox.y, box_url.boundingBox.width, box_url.boundingBox.height };

        for (int i = 0; i < ui.tab_count; i++) {
            Clay_ElementId id = Clay_GetElementIdWithIndex(CLAY_STRING("close_btn"), i);
            Clay_ElementData btn_close = Clay_GetElementData(id);
            ui.close_btns[i] = (SDL_Rect){ btn_close.boundingBox.x, btn_close.boundingBox.y, btn_close.boundingBox.width, btn_close.boundingBox.height };
        }

        SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
        SDL_RenderClear(rend);

        // render page layout constrained to viewport
        SDL_RenderSetViewport(rend, &viewport);
        clay_sdl_render(rend, saved_page_cmds);
        if (page_cmds_copy) free(page_cmds_copy);

        // render ui layout over the top
        SDL_Rect full_viewport = {0, 0, win_w, win_h};
        SDL_RenderSetViewport(rend, &full_viewport);
        clay_sdl_render(rend, ui_cmds);

        if (pending_load) {
            SDL_RenderPresent(rend);
            load_url(ui.tabs[ui.active_tab].url, (dom_node**)&ui.tabs[ui.active_tab].tree, 1, &ui.tabs[ui.active_tab].scroll_y, &focused_node, 1);
            strncpy(ui.input_buffer, ui.tabs[ui.active_tab].url, MAX_URL - 1);
            update_tab_title(&ui.tabs[ui.active_tab]);
            pending_load = 0;
            continue;
        }

        SDL_RenderPresent(rend);
        SDL_Delay(16);
    }

    for (int i = 0; i < ui.tab_count; i++) {
        if (ui.tabs[i].tree) {
            free_textures((dom_node*)ui.tabs[i].tree);
            free_tree((dom_node*)ui.tabs[i].tree);
        }
    }

    cleanup_renderer();
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
