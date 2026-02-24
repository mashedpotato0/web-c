#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "interactive_ui.h"

static int has_cls(dom_node *node, const char *cls) {
    const char *c = get_attribute(node, "class");
    return c && strstr(c, cls) != NULL;
}

void ui_init_document(dom_node *node) {
    if (!node) return;

    if (node->type == NODE_ELEMENT) {
        // collapse all heavy wikipedia sidebars and dropdowns by default
        if (has_cls(node, "vector-menu-content") ||
            has_cls(node, "vector-toc-contents") ||
            has_cls(node, "vector-dropdown-content")) {
            set_style(node, "display", "none");
            }

            // hide native checkboxes used for css-only toggling
            if (node->tag && strcasecmp(node->tag, "input") == 0) {
                const char *t = get_attribute(node, "type");
                if (t && strcasecmp(t, "checkbox") == 0) {
                    set_style(node, "display", "none");
                }
            }
    }

    for (int i = 0; i < node->child_count; i++) {
        ui_init_document(node->children[i]);
    }
}

static dom_node* ui_get_node_at_pos(dom_node *node, int x, int y_abs) {
    if (!node) return NULL;

    // traverse backwards to hit uppermost rendered elements first
    for (int i = node->child_count - 1; i >= 0; i--) {
        dom_node *hit = ui_get_node_at_pos(node->children[i], x, y_abs);
        if (hit) return hit;
    }

    if (node->type == NODE_ELEMENT && node->layout.w > 0 && node->layout.h > 0) {
        if (x >= node->layout.x && x <= node->layout.x + node->layout.w &&
            y_abs >= node->layout.y && y_abs <= node->layout.y + node->layout.h) {
            return node;
            }
    }
    return NULL;
}

int ui_handle_click(dom_node *root, int mx, int my, int scroll_y) {
    dom_node *hit = ui_get_node_at_pos(root, mx, my + scroll_y);
    if (!hit) return 0;

    dom_node *target = hit;
    while (target) {
        if (target->tag && (strcasecmp(target->tag, "label") == 0 ||
            has_cls(target, "vector-menu-heading") ||
            has_cls(target, "vector-toc-toggle"))) {
            break;
            }
            target = target->parent;
    }

    if (!target) return 0;

    printf("ui module handling click on interactive element\n");

    dom_node *p = target->parent;
    if (p) {
        for (int i = 0; i < p->child_count; i++) {
            dom_node *c = p->children[i];
            // toggle the sibling container content
            if (has_cls(c, "vector-menu-content") ||
                has_cls(c, "vector-dropdown-content") ||
                has_cls(c, "vector-toc-contents") ||
                (c->tag && strcasecmp(c->tag, "ul") == 0)) {

                const char *disp = get_style(c, "display");
            if (disp && strcmp(disp, "none") == 0) {
                set_style(c, "display", "block");
            } else {
                set_style(c, "display", "none");
            }
            return 1;
                }
        }
    }

    return 0;
}
