#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dom.h"

void decode_html_entities(char *str) {
    char *read = str;
    char *write = str;
    while (*read) {
        if (*read == '&') {
            if (strncmp(read, "&nbsp;", 6) == 0) { *write++ = ' '; read += 6; }
            else if (strncmp(read, "&amp;", 5) == 0) { *write++ = '&'; read += 5; }
            else if (strncmp(read, "&lt;", 4) == 0) { *write++ = '<'; read += 4; }
            else if (strncmp(read, "&gt;", 4) == 0) { *write++ = '>'; read += 4; }
            else if (strncmp(read, "&quot;", 6) == 0) { *write++ = '"'; read += 6; }
            else if (strncmp(read, "&copy;", 6) == 0) { *write++ = '\xC2'; *write++ = '\xA9'; read += 6; }
            else if (strncmp(read, "&raquo;", 7) == 0) { *write++ = '\xC2'; *write++ = '\xBB'; read += 7; }
            else if (strncmp(read, "&laquo;", 7) == 0) { *write++ = '\xC2'; *write++ = '\xAB'; read += 7; }
            else if (strncmp(read, "&#39;", 5) == 0) { *write++ = '\''; read += 5; }
            else if (strncmp(read, "&#", 2) == 0) {
                // parse numeric html entities
                read += 2;
                int val = 0;
                while (*read >= '0' && *read <= '9') {
                    val = val * 10 + (*read - '0');
                    read++;
                }
                if (*read == ';') read++;
                if (val > 0 && val < 128) { *write++ = (char)val; }
                else { *write++ = '?'; }
            }
            else { *write++ = *read++; }
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

dom_node* create_element(const char *tag, dom_node *parent) {
    dom_node *node = calloc(1, sizeof(dom_node));
    node->type = NODE_ELEMENT;

    char *tag_lower = strdup(tag);
    for(int i = 0; tag_lower[i]; i++) {
        tag_lower[i] = tolower((unsigned char)tag_lower[i]);
    }
    node->tag = tag_lower;

    node->parent = parent;
    node->child_capacity = 4;
    node->children = malloc(sizeof(dom_node*) * node->child_capacity);
    node->attr_capacity = 2;
    node->attributes = malloc(sizeof(dom_attr) * node->attr_capacity);
    node->style_capacity = 2;
    node->styles = malloc(sizeof(css_prop) * node->style_capacity);
    return node;
}

dom_node* create_text_node(const char *text, dom_node *parent) {
    dom_node *node = calloc(1, sizeof(dom_node));
    node->type = NODE_TEXT;
    node->text = strdup(text);
    node->parent = parent;
    return node;
}

void add_child(dom_node *parent, dom_node *child) {
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = realloc(parent->children, sizeof(dom_node*) * parent->child_capacity);
    }
    parent->children[parent->child_count++] = child;
}

void add_attribute(dom_node *node, const char *name, const char *value) {
    if (node->attr_count >= node->attr_capacity) {
        node->attr_capacity *= 2;
        node->attributes = realloc(node->attributes, sizeof(dom_attr) * node->attr_capacity);
    }
    node->attributes[node->attr_count].name = strdup(name);
    node->attributes[node->attr_count].value = value ? strdup(value) : NULL;

    if (strcasecmp(name, "href") == 0 && value) {
        node->href = strdup(value);
    }
    if (strcasecmp(name, "src") == 0 && value) {
        node->src = strdup(value);
    }

    node->attr_count++;
}

void set_attribute(dom_node *node, const char *name, const char *value) {
    if (!node || !name) return;
    for (int i = 0; i < node->attr_count; i++) {
        if (node->attributes[i].name && strcasecmp(node->attributes[i].name, name) == 0) {
            if (node->attributes[i].value) free(node->attributes[i].value);
            node->attributes[i].value = value ? strdup(value) : NULL;
            return;
        }
    }
    add_attribute(node, name, value);
}

const char* get_attribute(dom_node *node, const char *name) {
    if (!node || !node->attributes) return NULL;
    for (int i = 0; i < node->attr_count; i++) {
        if (node->attributes[i].name && strcasecmp(node->attributes[i].name, name) == 0) {
            return node->attributes[i].value;
        }
    }
    return NULL;
}

void set_style(dom_node *node, const char *name, const char *value) {
    if (!node || !name || !value) return;
    for (int i = 0; i < node->style_count; i++) {
        if (strcasecmp(node->styles[i].name, name) == 0) {
            free(node->styles[i].value);
            node->styles[i].value = strdup(value);
            return;
        }
    }
    if (node->style_count >= node->style_capacity) {
        node->style_capacity *= 2;
        node->styles = realloc(node->styles, sizeof(css_prop) * node->style_capacity);
    }
    node->styles[node->style_count].name = strdup(name);
    node->styles[node->style_count].value = strdup(value);
    node->style_count++;
}

const char* get_style(dom_node *node, const char *name) {
    if (!node || !node->styles) return NULL;
    for (int i = 0; i < node->style_count; i++) {
        if (node->styles[i].name && strcasecmp(node->styles[i].name, name) == 0) {
            return node->styles[i].value;
        }
    }
    return NULL;
}

void parse_attributes(dom_node *node, char *str) {
    while (*str) {
        while (*str && isspace((unsigned char)*str)) str++;
        if (!*str) break;

        char *name_start = str;
        while (*str && *str != '=' && !isspace((unsigned char)*str) && *str != '>') str++;

        char *name_end = str;
        char *val_start = NULL;

        if (*str == '=') {
            *name_end = '\0';
            str++;
            char quote = *str;
            if (quote == '"' || quote == '\'') {
                str++;
                val_start = str;
                while (*str && *str != quote) str++;
                if (*str) {
                    *str = '\0';
                    str++;
                }
            } else {
                val_start = str;
                while (*str && !isspace((unsigned char)*str) && *str != '>') str++;
                if (*str) {
                    *str = '\0';
                    str++;
                }
            }
        } else if (*str) {
            *name_end = '\0';
            str++;
        } else {
            *name_end = '\0';
        }

        if (strlen(name_start) > 0) {
            if (val_start) {
                decode_html_entities(val_start);
            }
            add_attribute(node, name_start, val_start);
        }
    }
}

dom_node* parse_html(const char *html) {
    dom_node *root = create_element("document", NULL);
    dom_node *current = root;

    int in_tag = 0;
    int buf_cap = 4194304;
    char *buffer = malloc(buf_cap);
    int buf_idx = 0;

    for (int i = 0; html[i] != '\0'; i++) {
        char c = html[i];

        if (c == '<') {
            if (current && current->tag &&
                (strcasecmp(current->tag, "style") == 0 || strcasecmp(current->tag, "script") == 0)) {
                if (strncmp(&html[i], "</style>", 8) != 0 && strncmp(&html[i], "</script>", 9) != 0 &&
                    strncmp(&html[i], "</STYLE>", 8) != 0 && strncmp(&html[i], "</SCRIPT>", 9) != 0) {
                    if (buf_idx < buf_cap - 1) buffer[buf_idx++] = c;
                    continue;
                    }
                }

                if (!in_tag && buf_idx > 0) {
                    buffer[buf_idx] = '\0';

                    char *r = buffer;
                    char *w = buffer;
                    int in_space = 0;
                    while (*r) {
                        if (isspace((unsigned char)*r)) {
                            if (!in_space) { *w++ = ' '; in_space = 1; }
                        } else {
                            *w++ = *r;
                            in_space = 0;
                        }
                        r++;
                    }
                    *w = '\0';

                    if (buffer[0] != '\0' && strcmp(buffer, " ") != 0) {
                        decode_html_entities(buffer);
                        dom_node *text_node = create_text_node(buffer, current);
                        add_child(current, text_node);
                    }
                    buf_idx = 0;
                }
                in_tag = 1;
        } else if (c == '>') {
            if (in_tag) {
                in_tag = 0;

                int is_self_closing = 0;
                if (buf_idx > 0 && buffer[buf_idx - 1] == '/') {
                    is_self_closing = 1;
                    buffer[buf_idx - 1] = '\0';
                } else {
                    buffer[buf_idx] = '\0';
                }

                if (buffer[0] == '/') {
                    if (current->parent != NULL) {
                        current = current->parent;
                    }
                } else if (buffer[0] == '!' || buffer[0] == '?') {
                } else {
                    char *tag_start = buffer;
                    while (*tag_start && isspace((unsigned char)*tag_start)) tag_start++;

                    char *space = strchr(tag_start, ' ');
                    if (space) {
                        *space = '\0';
                    }

                    dom_node *new_node = create_element(tag_start, current);

                    if (space) {
                        parse_attributes(new_node, space + 1);
                    }

                    add_child(current, new_node);

                    if (!is_self_closing &&
                        strcasecmp(tag_start, "meta") != 0 && strcasecmp(tag_start, "link") != 0 &&
                        strcasecmp(tag_start, "img") != 0 && strcasecmp(tag_start, "br") != 0 &&
                        strcasecmp(tag_start, "input") != 0 && strcasecmp(tag_start, "hr") != 0) {
                        current = new_node;
                        }
                }
                buf_idx = 0;
            } else {
                if (buf_idx < buf_cap - 1) buffer[buf_idx++] = c;
            }
        } else {
            if (buf_idx < buf_cap - 1) {
                buffer[buf_idx++] = c;
            }
        }
    }
    free(buffer);
    return root;
}

void print_tree(dom_node *root, int depth) {
    if (!root) return;
    for (int i = 0; i < depth; i++) printf("  ");
    if (root->type == NODE_ELEMENT) {
        printf("<%s>\n", root->tag);
    } else {
        printf("\"%s\"\n", root->text);
    }
    for (int i = 0; i < root->child_count; i++) {
        print_tree(root->children[i], depth + 1);
    }
}

void free_tree(dom_node *root) {
    if (!root) return;
    if (root->tag) free(root->tag);
    if (root->text) free(root->text);
    if (root->href) free(root->href);
    if (root->src) free(root->src);
    for (int i = 0; i < root->attr_count; i++) {
        if (root->attributes[i].name) free(root->attributes[i].name);
        if (root->attributes[i].value) free(root->attributes[i].value);
    }
    if (root->attributes) free(root->attributes);
    for (int i = 0; i < root->style_count; i++) {
        if (root->styles[i].name) free(root->styles[i].name);
        if (root->styles[i].value) free(root->styles[i].value);
    }
    if (root->styles) free(root->styles);
    for (int i = 0; i < root->child_count; i++) {
        free_tree(root->children[i]);
    }
    if (root->children) free(root->children);
    free(root);
}



