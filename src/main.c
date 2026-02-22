#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "fetcher.h"
#include "processor.h"
#include "renderer.h"
#include "css.h"

#define MAX_URL 8192

void load_url(char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets);

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

void submit_form(dom_node *node, char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets) {
    dom_node *form = node;
    while (form && (!form->tag || strcasecmp(form->tag, "form") != 0)) {
        form = form->parent;
    }
    if (!form) return;

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

            char *p_delim = strpbrk(start, "/?");
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

    load_url(url_buffer, tree, make_temp, scroll_y, focused_node, download_assets);
}

int check_click(dom_node *node, int mx, int my, char *url_buffer, dom_node **tree, int make_temp, int *scroll_y, dom_node **focused_node, int download_assets) {
    if (!node) return 0;

    int absolute_y = my + *scroll_y;
    int is_inside = (node->layout.w > 0 && node->layout.h > 0 &&
    mx >= node->layout.x && mx <= node->layout.x + node->layout.w &&
    absolute_y >= node->layout.y && absolute_y <= node->layout.y + node->layout.h);

    if (is_inside) {
        if (node->tag && strcasecmp(node->tag, "a") == 0 && node->href) {
            printf("clicked link: %s\n", node->href);

            if (node->href[0] == '#') {
                printf("scrolling to fragment: %s\n", node->href);
                dom_node *target = find_element_by_id(*tree, node->href + 1);
                if (target) {
                    *scroll_y = target->layout.y > 40 ? target->layout.y - 40 : 0;
                } else {
                    printf("fragment not found in dom\n");
                }
                return 1;
            }

            if (strncmp(node->href, "javascript:", 11) == 0 || strncmp(node->href, "mailto:", 7) == 0) {
                printf("ignoring unsupported protocol\n");
                return 1;
            }

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

                        char *p_delim = strpbrk(start, "/?");
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

            load_url(url_buffer, tree, make_temp, scroll_y, focused_node, download_assets);
            return 1;
        }

        if (node->tag && strcasecmp(node->tag, "input") == 0) {
            const char *type = get_attribute(node, "type");
            if (!type || strcasecmp(type, "text") == 0 || strcasecmp(type, "search") == 0) {
                *focused_node = node;
                return 1;
            }
            if (type && strcasecmp(type, "submit") == 0) {
                submit_form(node, url_buffer, tree, make_temp, scroll_y, focused_node, download_assets);
                return 1;
            }
        }

        if (node->tag && strcasecmp(node->tag, "button") == 0) {
            submit_form(node, url_buffer, tree, make_temp, scroll_y, focused_node, download_assets);
            return 1;
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
        if (strncmp(url_start, "http://", 7) == 0) {
            url_start += 7;
        } else if (strncmp(url_start, "https://", 8) == 0) {
            url_start += 8;
            port = "443";
        }

        char *p_delim = strpbrk(url_start, "/?");
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

        printf("fetching %s%s...\n", hostname, path);
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

                                printf("redirected to: %s\n", new_target);
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
                    char base_url[MAX_URL] = {0};
                    snprintf(base_url, sizeof(base_url), "%s://%s", strcmp(port, "443") == 0 ? "https" : "http", hostname);
                    printf("applying css styles...\n");
                    process_css(*tree, base_url, download_assets);
                    printf("downloading inline images...\n");
                    load_images(*tree, base_url, download_assets);
                }
                free(raw_data);
                break;
            }
        } else {
            printf("failed to fetch website data\n");
            break;
        }
    }
}

int main() {
    int make_temp = 1;
    int download_assets = 1;

    char url_buffer[MAX_URL] = "en.wikipedia.org/wiki/Donkey_Kong_(character)";
    dom_node *tree = NULL;
    int scroll_y = 0;
    dom_node *focused_node = NULL;

    printf("starting browser process...\n");
    if (init_renderer() != 0) {
        printf("fatal error: could not spin up the visual engine. exiting.\n");
        return 1;
    }

    SDL_PumpEvents();
    render_tree(NULL, "loading...", 0, NULL);

    load_url(url_buffer, &tree, make_temp, &scroll_y, &focused_node, download_assets);

    int running = 1;
    SDL_Event event;
    SDL_StartTextInput();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (event.button.y > 40) {
                        focused_node = NULL;
                        check_click(tree, event.button.x, event.button.y, url_buffer, &tree, make_temp, &scroll_y, &focused_node, download_assets);
                    } else {
                        focused_node = NULL;
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                scroll_y -= event.wheel.y * 30;
                if (scroll_y < 0) scroll_y = 0;
            } else if (event.type == SDL_TEXTINPUT) {
                if (focused_node) {
                    const char *val = get_attribute(focused_node, "value");
                    char new_val[MAX_URL] = {0};
                    if (val) strncpy(new_val, val, sizeof(new_val) - 2);
                    if (strlen(new_val) + strlen(event.text.text) < sizeof(new_val) - 1) {
                        strcat(new_val, event.text.text);
                        set_attribute(focused_node, "value", new_val);
                    }
                } else {
                    if (strlen(url_buffer) + strlen(event.text.text) < sizeof(url_buffer) - 1) {
                        strcat(url_buffer, event.text.text);
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE) {
                    if (focused_node) {
                        const char *val = get_attribute(focused_node, "value");
                        if (val && strlen(val) > 0) {
                            char new_val[MAX_URL] = {0};
                            strncpy(new_val, val, MAX_URL - 1);
                            new_val[strlen(new_val) - 1] = '\0';
                            set_attribute(focused_node, "value", new_val);
                        }
                    } else if (strlen(url_buffer) > 0) {
                        url_buffer[strlen(url_buffer) - 1] = '\0';
                    }
                } else if (event.key.keysym.sym == SDLK_RETURN) {
                    if (focused_node) {
                        submit_form(focused_node, url_buffer, &tree, make_temp, &scroll_y, &focused_node, download_assets);
                    } else {
                        load_url(url_buffer, &tree, make_temp, &scroll_y, &focused_node, download_assets);
                    }
                } else if (event.key.keysym.sym == SDLK_DOWN) {
                    scroll_y += 30;
                } else if (event.key.keysym.sym == SDLK_UP) {
                    scroll_y -= 30;
                    if (scroll_y < 0) scroll_y = 0;
                }
            }
        }

        render_tree(tree, url_buffer, scroll_y, focused_node);
        SDL_Delay(16);
    }

    printf("cleaning up and exiting...\n");
    if (tree) {
        free_textures(tree);
        free_tree(tree);
    }
    SDL_StopTextInput();
    cleanup_renderer();

    return 0;
}
