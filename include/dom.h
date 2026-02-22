#ifndef DOM_H
#define DOM_H

typedef enum {
    NODE_ELEMENT,
    NODE_TEXT
} node_type;

typedef struct {
    int x, y, w, h;
} rect;

typedef struct {
    char *name;
    char *value;
} dom_attr;

typedef struct {
    char *name;
    char *value;
} css_prop;

typedef struct dom_node {
    node_type type;
    char *tag;
    char *text;
    char *href;
    char *src;
    void *texture;
    int img_w, img_h;
    rect layout;

    dom_attr *attributes;
    int attr_count;
    int attr_capacity;

    css_prop *styles;
    int style_count;
    int style_capacity;

    struct dom_node **children;
    int child_count;
    int child_capacity;
    struct dom_node *parent;
} dom_node;

dom_node* create_element(const char *tag, dom_node *parent);
dom_node* create_text_node(const char *text, dom_node *parent);
void add_child(dom_node *parent, dom_node *child);
const char* get_attribute(dom_node *node, const char *name);
void set_attribute(dom_node *node, const char *name, const char *value);
void set_style(dom_node *node, const char *name, const char *value);
const char* get_style(dom_node *node, const char *name);
dom_node* parse_html(const char *html);
void print_tree(dom_node *root, int depth);
void free_tree(dom_node *root);

#endif
