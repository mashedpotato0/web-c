#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <duktape.h>
#include "js.h"
#include "dom.h"

static duk_context *js_ctx = NULL;
static dom_node *current_document = NULL;
static const char *current_base_url = NULL;

/* ===================== helpers ===================== */

static dom_node* find_by_id(dom_node *node, const char *id) {
    if (!node || !id) return NULL;
    if (node->type == NODE_ELEMENT) {
        const char *nid = get_attribute(node, "id");
        if (nid && strcmp(nid, id) == 0) return node;
    }
    for (int i = 0; i < node->child_count; i++) {
        dom_node *res = find_by_id(node->children[i], id);
        if (res) return res;
    }
    return NULL;
}

static dom_node* find_by_tag(dom_node *node, const char *tag) {
    if (!node || !tag) return NULL;
    if (node->type == NODE_ELEMENT && node->tag && strcasecmp(node->tag, tag) == 0) return node;
    for (int i = 0; i < node->child_count; i++) {
        dom_node *res = find_by_tag(node->children[i], tag);
        if (res) return res;
    }
    return NULL;
}

static int has_class_js(const char *cls_attr, const char *cls) {
    if (!cls_attr || !cls) return 0;
    int tlen = strlen(cls);
    const char *p = cls_attr;
    while ((p = strstr(p, cls)) != NULL) {
        int ok = (p == cls_attr || p[-1] == ' ') && (p[tlen] == '\0' || p[tlen] == ' ');
        if (ok) return 1;
        p += tlen;
    }
    return 0;
}

static dom_node* qs_node(dom_node *node, const char *sel) {
    if (!node || !sel) return NULL;
    if (node->type == NODE_ELEMENT && node->tag) {
        int match = 0;
        if (sel[0] == '#')      { const char *id = get_attribute(node,"id"); match = id && strcmp(id,sel+1)==0; }
        else if (sel[0] == '.') { const char *c  = get_attribute(node,"class"); match = has_class_js(c,sel+1); }
        else                    match = strcasecmp(node->tag, sel)==0;
        if (match) return node;
    }
    for (int i = 0; i < node->child_count; i++) {
        dom_node *res = qs_node(node->children[i], sel);
        if (res) return res;
    }
    return NULL;
}

typedef struct { dom_node *n; int i; } sf_t;

static dom_node* ptr_from_obj(duk_context *ctx, duk_idx_t idx) {
    if (!duk_is_object(ctx, idx)) return NULL;
    duk_get_prop_string(ctx, idx, "__ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1);
    duk_pop(ctx);
    return n;
}

/* ===================== forward decl ===================== */
static void push_element(duk_context *ctx, dom_node *node);

/* ===================== no-op stub ===================== */
static duk_ret_t js_noop(duk_context *ctx) { (void)ctx; return 0; }

/* ===================== element methods ===================== */

static duk_ret_t js_el_set_attribute(duk_context *ctx) {
    const char *name  = duk_safe_to_string(ctx, 0);
    const char *value = duk_safe_to_string(ctx, 1);
    duk_push_this(ctx);
    dom_node *n = ptr_from_obj(ctx, -1); duk_pop(ctx);
    if (n) set_attribute(n, name, value);
    return 0;
}
static duk_ret_t js_el_get_attribute(duk_context *ctx) {
    const char *name = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    dom_node *n = ptr_from_obj(ctx, -1); duk_pop(ctx);
    if (!n) { duk_push_null(ctx); return 1; }
    const char *v = get_attribute(n, name);
    if (v) duk_push_string(ctx, v); else duk_push_null(ctx);
    return 1;
}
static duk_ret_t js_el_set_style(duk_context *ctx) {
    const char *name  = duk_safe_to_string(ctx, 0);
    const char *value = duk_safe_to_string(ctx, 1);
    duk_push_this(ctx);
    dom_node *n = ptr_from_obj(ctx, -1); duk_pop(ctx);
    if (n) set_style(n, name, value);
    return 0;
}
static duk_ret_t js_el_get_style(duk_context *ctx) {
    const char *name = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    dom_node *n = ptr_from_obj(ctx, -1); duk_pop(ctx);
    if (!n) { duk_push_string(ctx, ""); return 1; }
    const char *v = get_style(n, name);
    duk_push_string(ctx, v ? v : "");
    return 1;
}
static duk_ret_t js_el_qs(duk_context *ctx) {
    const char *sel = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    dom_node *n = ptr_from_obj(ctx, -1); duk_pop(ctx);
    push_element(ctx, n ? qs_node(n, sel) : NULL);
    return 1;
}
static duk_ret_t js_el_qsa(duk_context *ctx) {
    (void)ctx; duk_push_array(ctx); return 1;
}
static duk_ret_t js_el_get_bounding_rect(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "top");
    duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "left");
    duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "bottom");
    duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "right");
    duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "width");
    duk_push_int(ctx, 0); duk_put_prop_string(ctx, -2, "height");
    return 1;
}

/* style object methods */
static duk_ret_t js_style_set_property(duk_context *ctx) {
    const char *name  = duk_safe_to_string(ctx, 0);
    const char *value = duk_safe_to_string(ctx, 1);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "__ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1); duk_pop_2(ctx);
    if (n) set_style(n, name, value);
    return 0;
}
static duk_ret_t js_style_get_property(duk_context *ctx) {
    const char *name = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "__ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1); duk_pop_2(ctx);
    if (!n) { duk_push_string(ctx, ""); return 1; }
    const char *v = get_style(n, name);
    duk_push_string(ctx, v ? v : "");
    return 1;
}

/* classList methods */
static duk_ret_t js_cl_contains(duk_context *ctx) {
    const char *cls = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "__el_ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1); duk_pop_2(ctx);
    if (!n) { duk_push_false(ctx); return 1; }
    duk_push_boolean(ctx, has_class_js(get_attribute(n,"class"), cls));
    return 1;
}
static duk_ret_t js_cl_add(duk_context *ctx) {
    const char *cls = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "__el_ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1); duk_pop_2(ctx);
    if (!n) return 0;
    const char *c = get_attribute(n, "class");
    if (!has_class_js(c, cls)) {
        char buf[1024] = {0};
        if (c && c[0]) snprintf(buf, 1023, "%s %s", c, cls);
        else strncpy(buf, cls, 1023);
        set_attribute(n, "class", buf);
    }
    return 0;
}
static duk_ret_t js_cl_remove(duk_context *ctx) {
    const char *cls = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "__el_ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1); duk_pop_2(ctx);
    if (!n) return 0;
    const char *c = get_attribute(n, "class");
    if (!c) return 0;
    char buf[2048]={0}, tmp[2048];
    strncpy(tmp, c, 2047);
    char *tok = strtok(tmp, " "); int first = 1;
    while (tok) {
        if (strcmp(tok,cls)!=0) { if(!first)strcat(buf," "); strcat(buf,tok); first=0; }
        tok = strtok(NULL, " ");
    }
    set_attribute(n, "class", buf);
    return 0;
}
static duk_ret_t js_cl_toggle(duk_context *ctx) {
    const char *cls = duk_safe_to_string(ctx, 0);
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "__el_ptr__");
    dom_node *n = (dom_node*)duk_get_pointer(ctx, -1); duk_pop_2(ctx);
    if (!n) { duk_push_false(ctx); return 1; }
    const char *c = get_attribute(n, "class");
    if (has_class_js(c, cls)) {
        char buf[2048]={0}, tmp[2048];
        strncpy(tmp, c ? c : "", 2047);
        char *tok = strtok(tmp, " "); int first=1;
        while (tok) {
            if (strcmp(tok,cls)!=0){if(!first)strcat(buf," ");strcat(buf,tok);first=0;}
            tok=strtok(NULL," ");
        }
        set_attribute(n,"class",buf);
        duk_push_false(ctx);
    } else {
        char buf[1024]={0};
        if(c&&c[0]) snprintf(buf,1023,"%s %s",c,cls); else strncpy(buf,cls,1023);
        set_attribute(n,"class",buf);
        duk_push_true(ctx);
    }
    return 1;
}

/* ===================== push_element ===================== */
static void push_element(duk_context *ctx, dom_node *node) {
    if (!node) { duk_push_null(ctx); return; }
    duk_push_object(ctx);

    duk_push_pointer(ctx, node); duk_put_prop_string(ctx, -2, "__ptr__");

    duk_push_string(ctx, node->tag ? node->tag : "#text"); duk_put_prop_string(ctx,-2,"tagName");
    duk_push_string(ctx, node->tag ? node->tag : "#text"); duk_put_prop_string(ctx,-2,"nodeName");

    const char *id  = node->tag ? get_attribute(node,"id")    : NULL;
    const char *cls = node->tag ? get_attribute(node,"class") : NULL;
    duk_push_string(ctx, id  ? id  : ""); duk_put_prop_string(ctx,-2,"id");
    duk_push_string(ctx, cls ? cls : ""); duk_put_prop_string(ctx,-2,"className");
    duk_push_string(ctx, node->href ? node->href : ""); duk_put_prop_string(ctx,-2,"href");
    duk_push_string(ctx, ""); duk_put_prop_string(ctx,-2,"innerHTML");
    duk_push_string(ctx, ""); duk_put_prop_string(ctx,-2,"textContent");
    duk_push_string(ctx, ""); duk_put_prop_string(ctx,-2,"innerText");
    duk_push_int(ctx, node->child_count); duk_put_prop_string(ctx,-2,"childElementCount");

    /* methods */
    duk_push_c_function(ctx, js_el_set_attribute,     2); duk_put_prop_string(ctx,-2,"setAttribute");
    duk_push_c_function(ctx, js_el_get_attribute,     1); duk_put_prop_string(ctx,-2,"getAttribute");
    duk_push_c_function(ctx, js_el_set_style,         2); duk_put_prop_string(ctx,-2,"setStyle");
    duk_push_c_function(ctx, js_el_get_style,         1); duk_put_prop_string(ctx,-2,"getStyle");
    duk_push_c_function(ctx, js_el_qs,                1); duk_put_prop_string(ctx,-2,"querySelector");
    duk_push_c_function(ctx, js_el_qsa,               1); duk_put_prop_string(ctx,-2,"querySelectorAll");
    duk_push_c_function(ctx, js_el_get_bounding_rect, 0); duk_put_prop_string(ctx,-2,"getBoundingClientRect");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"addEventListener");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"removeEventListener");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"appendChild");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"removeChild");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"insertBefore");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"replaceChild");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"dispatchEvent");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"focus");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"blur");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"click");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"scrollIntoView");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"insertAdjacentHTML");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"closest");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"matches");

    /* parentElement */
    push_element(ctx, node->parent);
    duk_put_prop_string(ctx, -2, "parentElement");

    /* style object */
    duk_push_object(ctx);
    duk_push_pointer(ctx, node); duk_put_prop_string(ctx,-2,"__ptr__");
    duk_push_c_function(ctx, js_style_set_property, 2); duk_put_prop_string(ctx,-2,"setProperty");
    duk_push_c_function(ctx, js_style_get_property, 1); duk_put_prop_string(ctx,-2,"getPropertyValue");
    duk_push_c_function(ctx, js_noop, 1); duk_put_prop_string(ctx,-2,"removeProperty");
    duk_put_prop_string(ctx, -2, "style");

    /* classList */
    duk_push_object(ctx);
    duk_push_pointer(ctx, node); duk_put_prop_string(ctx,-2,"__el_ptr__");
    duk_push_c_function(ctx, js_cl_contains, 1); duk_put_prop_string(ctx,-2,"contains");
    duk_push_c_function(ctx, js_cl_add,      1); duk_put_prop_string(ctx,-2,"add");
    duk_push_c_function(ctx, js_cl_remove,   1); duk_put_prop_string(ctx,-2,"remove");
    duk_push_c_function(ctx, js_cl_toggle,   1); duk_put_prop_string(ctx,-2,"toggle");
    duk_put_prop_string(ctx, -2, "classList");
}

/* ===================== document methods ===================== */

static duk_ret_t js_doc_gebi(duk_context *ctx) {
    push_element(ctx, find_by_id(current_document, duk_safe_to_string(ctx,0)));
    return 1;
}
static duk_ret_t js_doc_qs(duk_context *ctx) {
    push_element(ctx, qs_node(current_document, duk_safe_to_string(ctx,0)));
    return 1;
}
static duk_ret_t js_doc_qsa(duk_context *ctx) {
    const char *sel = duk_safe_to_string(ctx, 0);
    duk_push_array(ctx);
    int idx = 0;
    sf_t stack[4096]; int sp = 0;
    if (current_document) { stack[sp].n=current_document; stack[sp].i=0; sp++; }
    while (sp > 0) {
        dom_node *n = stack[sp-1].n; int ci = stack[sp-1].i;
        if (ci >= n->child_count) { sp--; continue; }
        dom_node *ch = n->children[ci]; stack[sp-1].i++;
        if (ch->type==NODE_ELEMENT && ch->tag) {
            int match=0;
            if (sel[0]=='#')      { const char *nid=get_attribute(ch,"id"); match=nid&&strcmp(nid,sel+1)==0; }
            else if (sel[0]=='.'){ const char *nc=get_attribute(ch,"class"); match=has_class_js(nc,sel+1); }
            else                   match=strcasecmp(ch->tag,sel)==0;
            if (match) { push_element(ctx,ch); duk_put_prop_index(ctx,-2,idx++); }
            if (sp<4094) { stack[sp].n=ch; stack[sp].i=0; sp++; }
        }
    }
    return 1;
}
static duk_ret_t js_doc_gebtn(duk_context *ctx) {
    const char *tag = duk_safe_to_string(ctx, 0);
    duk_push_array(ctx);
    int idx=0;
    sf_t stack[4096]; int sp=0;
    if (current_document) { stack[sp].n=current_document; stack[sp].i=0; sp++; }
    while (sp>0) {
        dom_node *n=stack[sp-1].n; int ci=stack[sp-1].i;
        if (ci>=n->child_count){sp--;continue;}
        dom_node *ch=n->children[ci]; stack[sp-1].i++;
        if (ch->type==NODE_ELEMENT&&ch->tag) {
            if (strcmp(tag,"*")==0||strcasecmp(ch->tag,tag)==0)
                { push_element(ctx,ch); duk_put_prop_index(ctx,-2,idx++); }
            if (sp<4094){stack[sp].n=ch;stack[sp].i=0;sp++;}
        }
    }
    return 1;
}
static duk_ret_t js_doc_gebcn(duk_context *ctx) {
    const char *cls = duk_safe_to_string(ctx, 0);
    duk_push_array(ctx);
    int idx=0;
    sf_t stack[4096]; int sp=0;
    if (current_document) { stack[sp].n=current_document; stack[sp].i=0; sp++; }
    while (sp>0) {
        dom_node *n=stack[sp-1].n; int ci=stack[sp-1].i;
        if (ci>=n->child_count){sp--;continue;}
        dom_node *ch=n->children[ci]; stack[sp-1].i++;
        if (ch->type==NODE_ELEMENT&&ch->tag) {
            if (has_class_js(get_attribute(ch,"class"),cls))
                { push_element(ctx,ch); duk_put_prop_index(ctx,-2,idx++); }
            if (sp<4094){stack[sp].n=ch;stack[sp].i=0;sp++;}
        }
    }
    return 1;
}
static duk_ret_t js_doc_create_el(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_string(ctx, duk_safe_to_string(ctx,0)); duk_put_prop_string(ctx,-2,"tagName");
    duk_push_object(ctx); duk_put_prop_string(ctx,-2,"style");
    duk_push_c_function(ctx,js_noop,DUK_VARARGS); duk_put_prop_string(ctx,-2,"addEventListener");
    duk_push_c_function(ctx,js_noop,DUK_VARARGS); duk_put_prop_string(ctx,-2,"setAttribute");
    duk_push_c_function(ctx,js_noop,DUK_VARARGS); duk_put_prop_string(ctx,-2,"appendChild");
    return 1;
}
static duk_ret_t js_doc_create_tn(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_string(ctx,"#text"); duk_put_prop_string(ctx,-2,"nodeName");
    duk_push_string(ctx,duk_safe_to_string(ctx,0)); duk_put_prop_string(ctx,-2,"textContent");
    return 1;
}
static duk_ret_t js_doc_create_frag(duk_context *ctx) {
    duk_push_object(ctx);
    duk_push_c_function(ctx,js_noop,DUK_VARARGS); duk_put_prop_string(ctx,-2,"appendChild");
    return 1;
}

static void setup_document(duk_context *ctx) {
    duk_push_object(ctx);

    duk_push_c_function(ctx, js_doc_gebi,      1); duk_put_prop_string(ctx,-2,"getElementById");
    duk_push_c_function(ctx, js_doc_qs,        1); duk_put_prop_string(ctx,-2,"querySelector");
    duk_push_c_function(ctx, js_doc_qsa,       1); duk_put_prop_string(ctx,-2,"querySelectorAll");
    duk_push_c_function(ctx, js_doc_gebtn,     1); duk_put_prop_string(ctx,-2,"getElementsByTagName");
    duk_push_c_function(ctx, js_doc_gebcn,     1); duk_put_prop_string(ctx,-2,"getElementsByClassName");
    duk_push_c_function(ctx, js_doc_create_el, 1); duk_put_prop_string(ctx,-2,"createElement");
    duk_push_c_function(ctx, js_doc_create_tn, 1); duk_put_prop_string(ctx,-2,"createTextNode");
    duk_push_c_function(ctx, js_doc_create_frag,0);duk_put_prop_string(ctx,-2,"createDocumentFragment");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"addEventListener");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"removeEventListener");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"write");
    duk_push_c_function(ctx, js_noop, DUK_VARARGS); duk_put_prop_string(ctx,-2,"writeln");

    dom_node *body = current_document ? find_by_tag(current_document,"body") : NULL;
    push_element(ctx, body ? body : current_document); duk_put_prop_string(ctx,-2,"body");
    push_element(ctx, current_document); duk_put_prop_string(ctx,-2,"documentElement");

    duk_push_string(ctx,"");         duk_put_prop_string(ctx,-2,"cookie");
    duk_push_string(ctx,"");         duk_put_prop_string(ctx,-2,"title");
    duk_push_string(ctx,"complete"); duk_put_prop_string(ctx,-2,"readyState");
    duk_push_string(ctx, current_base_url ? current_base_url : ""); duk_put_prop_string(ctx,-2,"URL");
    duk_push_string(ctx, current_base_url ? current_base_url : ""); duk_put_prop_string(ctx,-2,"documentURI");
    duk_push_string(ctx,"BackCompat"); duk_put_prop_string(ctx,-2,"compatMode");
    duk_push_string(ctx,"UTF-8");     duk_put_prop_string(ctx,-2,"characterSet");

    duk_put_global_string(ctx, "document");
}

/* ===================== print / native bridge ===================== */

static duk_ret_t native_print(duk_context *ctx) {
    printf("js: %s\n", duk_safe_to_string(ctx, 0));
    return 0;
}
static duk_ret_t native_hide_element(duk_context *ctx) {
    dom_node *n = find_by_id(current_document, duk_safe_to_string(ctx,0));
    if (n) set_style(n,"display","none");
    return 0;
}
static duk_ret_t native_show_element(duk_context *ctx) {
    dom_node *n = find_by_id(current_document, duk_safe_to_string(ctx,0));
    if (n) set_style(n,"display","block");
    return 0;
}
static duk_ret_t native_set_style(duk_context *ctx) {
    dom_node *n = find_by_id(current_document, duk_safe_to_string(ctx,0));
    if (n) set_style(n, duk_safe_to_string(ctx,1), duk_safe_to_string(ctx,2));
    return 0;
}
static duk_ret_t native_set_attribute(duk_context *ctx) {
    dom_node *n = find_by_id(current_document, duk_safe_to_string(ctx,0));
    if (n) set_attribute(n, duk_safe_to_string(ctx,1), duk_safe_to_string(ctx,2));
    return 0;
}

/* ===================== init_js ===================== */
int init_js() {
    js_ctx = duk_create_heap_default();
    if (!js_ctx) { printf("fatal: failed to init duktape\n"); return -1; }

    /* console */
    duk_push_object(js_ctx);
    duk_push_c_function(js_ctx,native_print,1); duk_put_prop_string(js_ctx,-2,"log");
    duk_push_c_function(js_ctx,native_print,1); duk_put_prop_string(js_ctx,-2,"error");
    duk_push_c_function(js_ctx,native_print,1); duk_put_prop_string(js_ctx,-2,"warn");
    duk_push_c_function(js_ctx,native_print,1); duk_put_prop_string(js_ctx,-2,"info");
    duk_push_c_function(js_ctx,native_print,1); duk_put_prop_string(js_ctx,-2,"debug");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"group");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"groupCollapsed");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"groupEnd");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"time");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"timeEnd");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"assert");
    duk_push_c_function(js_ctx,js_noop,DUK_VARARGS); duk_put_prop_string(js_ctx,-2,"table");
    duk_put_global_string(js_ctx, "console");

    duk_push_c_function(js_ctx,native_print,1); duk_put_global_string(js_ctx,"print");
    duk_push_c_function(js_ctx,native_hide_element,  1); duk_put_global_string(js_ctx,"hideElementById");
    duk_push_c_function(js_ctx,native_show_element,  1); duk_put_global_string(js_ctx,"showElementById");
    duk_push_c_function(js_ctx,native_set_style,     3); duk_put_global_string(js_ctx,"setElementStyle");
    duk_push_c_function(js_ctx,native_set_attribute, 3); duk_put_global_string(js_ctx,"setElementAttribute");

    /* timer stubs */
    duk_eval_string_noresult(js_ctx,
        "function setTimeout(fn,ms){return 0;}\n"
        "function clearTimeout(id){}\n"
        "function setInterval(fn,ms){return 0;}\n"
        "function clearInterval(id){}\n"
        "function requestAnimationFrame(fn){return 0;}\n"
        "function cancelAnimationFrame(id){}\n"
        "function queueMicrotask(fn){}\n"
    );

    /* window + globals */
    duk_eval_string_noresult(js_ctx,
        "var window=globalThis;\n"
        "window.location={href:'',pathname:'/',search:'',hash:'',hostname:'',protocol:'https:',"
        "  assign:function(){},replace:function(){},reload:function(){}};\n"
        "window.history={pushState:function(){},replaceState:function(){},back:function(){},forward:function(){}};\n"
        "window.screen={width:1280,height:720,availWidth:1280,availHeight:720};\n"
        "window.innerWidth=1280;window.innerHeight=720;\n"
        "window.outerWidth=1280;window.outerHeight=720;\n"
        "window.pageXOffset=0;window.pageYOffset=0;\n"
        "window.scrollX=0;window.scrollY=0;\n"
        "window.devicePixelRatio=1;\n"
        "window.addEventListener=function(){};\n"
        "window.removeEventListener=function(){};\n"
        "window.dispatchEvent=function(){};\n"
        "window.getComputedStyle=function(el,p){"
        "  return{getPropertyValue:function(n){return el&&el.getStyle?el.getStyle(n):'';},"
        "         display:'',visibility:'',opacity:'1'};};\n"
        "window.matchMedia=function(q){return{matches:false,addListener:function(){},removeEventListener:function(){}}};\n"
        "window.performance={now:function(){return Date.now();},mark:function(){},measure:function(){},"
        "  getEntriesByType:function(){return[];}};\n"
        "window.localStorage={getItem:function(){return null;},setItem:function(){},removeItem:function(){},clear:function(){},length:0};\n"
        "window.sessionStorage={getItem:function(){return null;},setItem:function(){},removeItem:function(){},clear:function(){},length:0};\n"
        "window.JSON=JSON;\n"
        "window.MutationObserver=function(cb){this.observe=function(){};this.disconnect=function(){};this.takeRecords=function(){return[]};};\n"
        "window.IntersectionObserver=function(cb,o){this.observe=function(){};this.unobserve=function(){};this.disconnect=function(){};};\n"
        "window.ResizeObserver=function(cb){this.observe=function(){};this.unobserve=function(){};this.disconnect=function(){};};\n"
        "var navigator={userAgent:'Mozilla/5.0 C-Browser/2.0',language:'en-US',languages:['en-US'],"
        "  platform:'Linux',cookieEnabled:false,onLine:true,vendor:'',appName:'Netscape'};\n"
        "var XMLHttpRequest=function(){"
        "  this.open=function(){};this.send=function(){};this.setRequestHeader=function(){};"
        "  this.abort=function(){};this.readyState=0;this.status=0;this.responseText='';};\n"
        "var fetch=function(){return{then:function(fn){return this;},catch:function(){return this;}};};\n"
        "var CustomEvent=function(t,o){this.type=t;this.detail=(o&&o.detail)||null;};\n"
        "var Event=function(t){this.type=t;this.preventDefault=function(){};this.stopPropagation=function(){};this.stopImmediatePropagation=function(){};};\n"
        "var KeyboardEvent=Event;var MouseEvent=Event;var TouchEvent=Event;var UIEvent=Event;var FocusEvent=Event;\n"
        "var WeakMap=function(){this.get=function(){return undefined;};this.set=function(){return this;};this.has=function(){return false;};this.delete=function(){};};\n"
        "var WeakSet=function(){this.add=function(){return this;};this.has=function(){return false;};this.delete=function(){};};\n"
    );

    return 0;
}

/* ===================== run_js ===================== */

static void execute_scripts(dom_node *node, const char *base_url) {
    if (!node) return;

    if (node->type == NODE_ELEMENT && node->tag && strcasecmp(node->tag,"script")==0) {
        const char *type = get_attribute(node,"type");
        if (type && (strcasecmp(type,"text/template")==0 ||
                     strcasecmp(type,"application/ld+json")==0 ||
                     strstr(type,"x-tmpl") != NULL)) goto children;

        if (!get_attribute(node,"src") &&
            node->child_count > 0 &&
            node->children[0]->type == NODE_TEXT) {

            setup_document(js_ctx);
            if (base_url) {
                duk_get_global_string(js_ctx,"window");
                duk_get_prop_string(js_ctx,-1,"location");
                duk_push_string(js_ctx,base_url);
                duk_put_prop_string(js_ctx,-2,"href");
                duk_pop_2(js_ctx);
            }
            if (duk_peval_string(js_ctx, node->children[0]->text) != 0) {
                /* swallow — expected with incomplete DOM */
            }
            duk_pop(js_ctx);
        }
    }

children:
    for (int i = 0; i < node->child_count; i++)
        execute_scripts(node->children[i], base_url);
}

void run_js(dom_node *root, const char *base_url) {
    if (!js_ctx || !root) return;
    current_document = root;
    current_base_url = base_url;
    setup_document(js_ctx);
    printf("executing javascript...\n");
    execute_scripts(root, base_url);
    printf("javascript done.\n");
}

void cleanup_js() {
    if (js_ctx) { duk_destroy_heap(js_ctx); js_ctx = NULL; }
}
