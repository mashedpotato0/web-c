#ifndef DOM_H
#define DOM_H

typedef enum {
    NODE_ELEMENT,
    NODE_TEXT
} node_type;

typedef struct {
    int x, y, w, h;
} rect;

typedef struct dom_node {
    node_type type;
    char *tag;
    char *text;
    char *href;
    rect layout;
    struct dom_node **children;
    int child_count;
    int child_capacity;
    struct dom_node *parent;
} dom_node;

dom_node* create_element(const char *tag, dom_node *parent);
dom_node* create_text_node(const char *text, dom_node *parent);
void add_child(dom_node *parent, dom_node *child);
dom_node* parse_html(const char *html);
void print_tree(dom_node *root, int depth);
void free_tree(dom_node *root);

#endif
