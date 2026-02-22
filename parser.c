#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dom.h"

dom_node* create_element(const char *tag, dom_node *parent) {
    dom_node *node = calloc(1, sizeof(dom_node));
    node->type = NODE_ELEMENT;
    node->tag = strdup(tag);
    node->parent = parent;
    node->child_capacity = 4;
    node->children = malloc(sizeof(dom_node*) * node->child_capacity);
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

dom_node* parse_html(const char *html) {
    dom_node *root = create_element("document", NULL);
    dom_node *current = root;

    int in_tag = 0;
    char buffer[8192];
    int buf_idx = 0;

    for (int i = 0; html[i] != '\0'; i++) {
        char c = html[i];

        if (c == '<') {
            if (!in_tag && buf_idx > 0) {
                buffer[buf_idx] = '\0';
                int is_empty = 1;
                for (int j = 0; j < buf_idx; j++) {
                    if (!isspace(buffer[j])) is_empty = 0;
                }
                if (!is_empty) {
                    dom_node *text_node = create_text_node(buffer, current);
                    add_child(current, text_node);
                }
                buf_idx = 0;
            }
            in_tag = 1;
        } else if (c == '>') {
            in_tag = 0;
            buffer[buf_idx] = '\0';

            if (buffer[0] == '/') {
                if (current->parent != NULL) {
                    current = current->parent;
                }
            } else if (buffer[0] == '!' || buffer[0] == '?') {
            } else {
                char *space = strchr(buffer, ' ');
                char *href_val = NULL;

                if (space) {
                    *space = '\0';

                    char *href_start = strstr(space + 1, "href=\"");
                    if (href_start) {
                        href_start += 6;
                        char *href_end = strchr(href_start, '"');
                        if (href_end) {
                            *href_end = '\0';
                            href_val = strdup(href_start);
                        }
                    }
                }

                dom_node *new_node = create_element(buffer, current);
                if (href_val) new_node->href = href_val;

                add_child(current, new_node);

                if (strcmp(buffer, "meta") != 0 && strcmp(buffer, "link") != 0 &&
                    strcmp(buffer, "img") != 0 && strcmp(buffer, "br") != 0) {
                    current = new_node;
                    }
            }
            buf_idx = 0;
        } else {
            if (buf_idx < 8191) {
                buffer[buf_idx++] = c;
            }
        }
    }
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
    for (int i = 0; i < root->child_count; i++) {
        free_tree(root->children[i]);
    }
    if (root->children) free(root->children);
    free(root);
}
