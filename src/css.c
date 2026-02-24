#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "css.h"
#include "dom.h"
#include "fetcher.h"

typedef struct {
    char *selector;
    char *prop;
    char *val;
    int specificity;
    int order;
} css_rule_t;

typedef struct {
    char *name;
    char *val;
} css_var_t;

static css_rule_t *rules = NULL;
static int rule_count = 0;
static int rule_cap = 0;
static int rule_order = 0;

static css_var_t *css_vars = NULL;
static int css_var_count = 0;
static int css_var_cap = 0;

static void add_css_var(const char *name, const char *val) {
    for (int i = 0; i < css_var_count; i++) {
        if (strcasecmp(css_vars[i].name, name) == 0) {
            free(css_vars[i].val);
            css_vars[i].val = strdup(val);
            return;
        }
    }
    if (css_var_count >= css_var_cap) {
        css_var_cap = css_var_cap == 0 ? 1024 : css_var_cap * 2;
        css_vars = realloc(css_vars, css_var_cap * sizeof(css_var_t));
    }
    css_vars[css_var_count].name = strdup(name);
    css_vars[css_var_count].val = strdup(val);
    css_var_count++;
}

static const char* get_css_var(const char *name) {
    for (int i = 0; i < css_var_count; i++) {
        if (strcasecmp(css_vars[i].name, name) == 0) return css_vars[i].val;
    }
    return NULL;
}

static int calc_specificity(const char *sel) {
    int a = 0, b = 0, c = 0;
    const char *p = sel;
    while (*p) {
        if (*p == '#') { a++; p++; while (*p && !strchr(" .#:[>+~", *p)) p++; }
        else if (*p == '.') { b++; p++; while (*p && !strchr(" .#:[>+~", *p)) p++; }
        else if (*p == '[') { b++; while (*p && *p != ']') p++; if (*p) p++; }
        else if (*p == ':') {
            p++;
            if (*p == ':') { c++; p++; } else { b++; }
            while (*p && !strchr(" .#:[>+~", *p) && *p != '(') p++;
            if (*p == '(') { int d=1; p++; while(*p && d>0) { if(*p=='(') d++; if(*p==')') d--; p++; } }
        }
        else if (isalpha((unsigned char)*p) || *p == '_') {
            c++;
            while (*p && !strchr(" .#:[>+~", *p)) p++;
        }
        else { p++; }
    }
    return a * 10000 + b * 100 + c;
}

static void add_rule(const char *sel, const char *prop, const char *val) {
    if (strncmp(prop, "--", 2) == 0) {
        if (strcasecmp(sel, ":root") == 0 || strcasecmp(sel, "html") == 0 || strcasecmp(sel, "body") == 0) {
            add_css_var(prop, val);
        }
        return;
    }

    if (rule_count >= rule_cap) {
        rule_cap = rule_cap == 0 ? 2048 : rule_cap * 2;
        rules = realloc(rules, rule_cap * sizeof(css_rule_t));
    }
    rules[rule_count].selector = strdup(sel);
    rules[rule_count].prop = strdup(prop);
    rules[rule_count].val = strdup(val);
    rules[rule_count].specificity = calc_specificity(sel);
    rules[rule_count].order = rule_order++;
    rule_count++;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

typedef struct { const char *name; uint8_t r, g, b; } named_color_t;
static const named_color_t named_colors[] = {
    {"aliceblue",240,248,255}, {"black",0,0,0}, {"blue",0,0,255},
    {"gray",128,128,128}, {"green",0,128,0}, {"red",255,0,0},
    {"white",255,255,255}, {"yellow",255,255,0}, {"transparent",0,0,0},
    {NULL,0,0,0}
};

SDL_Color parse_css_color(const char *color_str, SDL_Color def) {
    if (!color_str) return def;
    while(isspace((unsigned char)*color_str)) color_str++;
    if (!*color_str || strcasecmp(color_str, "transparent") == 0) return (SDL_Color){0,0,0,0};
    if (strcasecmp(color_str, "inherit") == 0 || strcasecmp(color_str, "currentcolor") == 0) return def;

    if (color_str[0] == '#') {
        int len = 0;
        while(isxdigit((unsigned char)color_str[1+len])) len++;
        if (len == 3) {
            return (SDL_Color){hex_val(color_str[1])*17, hex_val(color_str[2])*17, hex_val(color_str[3])*17, 255};
        } else if (len == 6) {
            return (SDL_Color){
                hex_val(color_str[1])*16 + hex_val(color_str[2]),
                hex_val(color_str[3])*16 + hex_val(color_str[4]),
                hex_val(color_str[5])*16 + hex_val(color_str[6]), 255};
        }
    }

    if (strncasecmp(color_str, "rgba", 4) == 0 || strncasecmp(color_str, "rgb", 3) == 0) {
        int r=0,g=0,b=0; float a_f=1.0f;
        const char *p = strchr(color_str, '(');
        if (p) {
            p++; r = atoi(p);
            p = strchr(p, ','); if(p){ p++; g = atoi(p); }
            p = p ? strchr(p, ',') : NULL; if(p){ p++; b = atoi(p); }
            p = p ? strchr(p, ',') : NULL; if(p){ p++; a_f = atof(p); }
        }
        return (SDL_Color){r,g,b,(int)(a_f*255)};
    }

    for (int i = 0; named_colors[i].name; i++) {
        if (strcasecmp(color_str, named_colors[i].name) == 0)
            return (SDL_Color){named_colors[i].r, named_colors[i].g, named_colors[i].b, strcasecmp(color_str, "transparent") == 0 ? 0 : 255};
    }
    return def;
}

static void expand_shorthand(const char *sel, const char *prop, const char *val) {
    int is_margin  = (strcasecmp(prop, "margin") == 0);
    int is_padding = (strcasecmp(prop, "padding") == 0);
    if (is_margin || is_padding) {
        const char *prefix = is_margin ? "margin" : "padding";
        char parts[4][64] = {0};
        int n = 0;
        char copy[512]; strncpy(copy, val, 511);
        char *tok = strtok(copy, " ");
        while (tok && n < 4) { strncpy(parts[n++], tok, 63); tok = strtok(NULL, " "); }
        const char *top, *right, *bottom, *left;
        if (n == 1)      { top=parts[0]; right=parts[0]; bottom=parts[0]; left=parts[0]; }
        else if (n == 2) { top=parts[0]; right=parts[1]; bottom=parts[0]; left=parts[1]; }
        else if (n == 3) { top=parts[0]; right=parts[1]; bottom=parts[2]; left=parts[1]; }
        else             { top=parts[0]; right=parts[1]; bottom=parts[2]; left=parts[3]; }
        char buf[128];
        snprintf(buf, sizeof(buf), "%s-top", prefix);    add_rule(sel, buf, top);
        snprintf(buf, sizeof(buf), "%s-right", prefix);  add_rule(sel, buf, right);
        snprintf(buf, sizeof(buf), "%s-bottom", prefix); add_rule(sel, buf, bottom);
        snprintf(buf, sizeof(buf), "%s-left", prefix);   add_rule(sel, buf, left);
        return;
    }
    add_rule(sel, prop, val);
}

static void parse_css_text(const char *css);

static void parse_css_block(const char *selector, const char *props) {
    char *save_sel;
    char sel_copy[1024]; strncpy(sel_copy, selector, 1023);
    char *sel = strtok_r(sel_copy, ",", &save_sel);
    while (sel) {
        while(isspace((unsigned char)*sel)) sel++;
        char *end = sel + strlen(sel) - 1;
        while(end > sel && isspace((unsigned char)*end)) { *end = '\0'; end--; }

        if (strlen(sel) > 0) {
            char *props_copy = strdup(props);
            char *save_prop;
            char *prop_token = strtok_r(props_copy, ";", &save_prop);
            while (prop_token) {
                char *colon = strchr(prop_token, ':');
                if (colon && !strchr(prop_token, '{')) {
                    *colon = '\0';
                    char *pname = prop_token;
                    char *pval  = colon + 1;
                    while(isspace((unsigned char)*pname)) pname++;
                    char *pend = pname + strlen(pname) - 1;
                    while(pend > pname && isspace((unsigned char)*pend)) { *pend='\0'; pend--; }
                    while(isspace((unsigned char)*pval)) pval++;
                    pend = pval + strlen(pval) - 1;
                    while(pend > pval && isspace((unsigned char)*pend)) { *pend='\0'; pend--; }
                    char *imp = strstr(pval, "!important");
                    if (imp) { *imp = '\0'; pend = pval+strlen(pval)-1; while(pend>pval&&isspace((unsigned char)*pend)){*pend='\0';pend--;} }

                    if (strlen(pname) > 0 && strlen(pval) > 0) expand_shorthand(sel, pname, pval);
                }
                prop_token = strtok_r(NULL, ";", &save_prop);
            }
            free(props_copy);
        }
        sel = strtok_r(NULL, ",", &save_sel);
    }
}

static void parse_css_text(const char *css) {
    int state = 0;
    char selector[1024] = {0};
    char *props = malloc(1048576);
    int s_idx = 0, p_idx = 0;
    int depth = 0;

    for (int i = 0; css[i]; i++) {
        char c = css[i];
        if (c == '/' && css[i+1] == '*') {
            i += 2;
            while (css[i] && !(css[i] == '*' && css[i+1] == '/')) i++;
            if (css[i]) i++;
            continue;
        }
        if (state == 0) {
            if (c == '{') {
                selector[s_idx] = '\0';
                int t = s_idx - 1;
                while (t >= 0 && isspace((unsigned char)selector[t])) selector[t--] = '\0';
                state = 1; p_idx = 0; depth = 1;
            } else {
                if (s_idx == 0 && isspace((unsigned char)c)) continue;
                if (s_idx < 1023) selector[s_idx++] = c;
            }
        } else if (state == 1) {
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) { props[p_idx] = '\0'; goto emit; } }
            if (p_idx < 1048575) props[p_idx++] = c;
            continue;
            emit:
            if (strchr(selector, '@') != NULL) {
                if (strstr(selector, "media")) {
                    if (!strstr(selector, "print") && !strstr(selector, "max-width: 7") && !strstr(selector, "max-width: 4")) {
                        parse_css_text(props);
                    }
                } else if (strstr(selector, "supports")) {
                    parse_css_text(props);
                }
            } else {
                parse_css_block(selector, props);
            }
            state = 0; s_idx = 0;
            memset(selector, 0, sizeof(selector));
        }
    }
    free(props);
}

static int has_class(const char *cls_attr, const char *target) {
    if (!cls_attr || !target) return 0;
    int tlen = strlen(target);
    const char *p = cls_attr;
    while ((p = strstr(p, target)) != NULL) {
        int start_ok = (p == cls_attr || isspace((unsigned char)p[-1]));
        int end_ok   = (p[tlen] == '\0' || isspace((unsigned char)p[tlen]));
        if (start_ok && end_ok) return 1;
        p += tlen;
    }
    return 0;
}

static int match_simple(dom_node *node, const char *sel) {
    if (!node || node->type != NODE_ELEMENT || !node->tag || !sel) return 0;
    if (strcmp(sel, "*") == 0) return 1;
    char buf[512]; strncpy(buf, sel, 511);
    char *pseudo = NULL;
    for (char *p = buf; *p; p++) if (*p == ':' && pseudo == NULL) pseudo = p;
    if (pseudo) *pseudo = '\0';

    char *p = buf;
    char tag_part[128] = {0};
    int ti = 0;
    while (*p && *p != '.' && *p != '#' && *p != '[') tag_part[ti++] = *p++;

    if (strlen(tag_part) > 0 && strcmp(tag_part,"*") != 0) {
        if (strcasecmp(node->tag, tag_part) != 0) return 0;
    }

    while (*p) {
        if (*p == '.') {
            p++;
            char cls[128]={0}; int ci=0;
            while(*p && *p!='.' && *p!='#' && *p!='[') cls[ci++] = *p++;
            if (!has_class(get_attribute(node, "class"), cls)) return 0;
        } else if (*p == '#') {
            p++;
            char id[128]={0}; int ii=0;
            while(*p && *p!='.' && *p!='#' && *p!='[') id[ii++] = *p++;
            const char *av = get_attribute(node, "id");
            if (!av || strcmp(av, id)!=0) return 0;
        } else {
            p++;
        }
    }
    return 1;
}

static int matches_selector(dom_node *node, const char *selector) {
    if (!node || !node->tag || !selector) return 0;
    char sel_copy[1024]; strncpy(sel_copy, selector, 1023);
    char *p = sel_copy;
    int depth = 0;
    char tokens[64][256]; int tc=0;
    char cur[256]={0}; int ci=0;

    for (; *p; p++) {
        if (*p=='[') depth++; else if (*p==']') depth--;
        if (depth > 0) { if(ci<255) cur[ci++]=*p; continue; }
        if (*p==' '||*p=='>'||*p=='+'||*p=='~') {
            if (ci > 0 && tc < 63) { cur[ci]='\0'; strncpy(tokens[tc++],cur,255); ci=0; }
            memset(cur,0,sizeof(cur));
        } else {
            if(ci<255) cur[ci++]=*p;
        }
    }
    if (ci>0) { cur[ci]='\0'; strncpy(tokens[tc],cur,255); tc++; }
    if (tc==0) return 0;
    if (!match_simple(node, tokens[tc-1])) return 0;
    if (tc==1) return 1;

    dom_node *curr = node->parent;
    int pi = tc-2;
    while (pi >= 0 && curr) {
        if (curr->type == NODE_ELEMENT && curr->tag) {
            if (match_simple(curr, tokens[pi])) pi--;
        }
        curr = curr->parent;
    }
    return pi < 0;
}

static const char* resolve_var(const char *val, char *buffer, int buf_size) {
    if (!val) return val;
    char tmp[1024]; strncpy(tmp, val, 1023);
    for (int pass = 0; pass < 4; pass++) {
        if (strncmp(tmp, "var(", 4) != 0 && !strstr(tmp, "var(")) break;
        char out[1024]={0}; int oi=0;
        const char *p = tmp;
        while (*p && oi < 1023) {
            if (strncmp(p,"var(",4)==0) {
                p += 4;
                char vname[256]={0}; int vi=0;
                while(*p && *p!=')' && *p!=',' && vi<255) vname[vi++]=*p++;
                char fallback[256]={0}; int fi=0;
                if (*p==',') { p++; while(isspace((unsigned char)*p))p++; while(*p&&*p!=')'&&fi<255) fallback[fi++]=*p++; }
                if (*p==')') p++;
                const char *rv = get_css_var(vname);
                const char *use = rv ? rv : (fi>0?fallback:vname);
                int ul = strlen(use);
                if (oi+ul < 1023) { memcpy(out+oi, use, ul); oi+=ul; }
            } else {
                out[oi++] = *p++;
            }
        }
        out[oi]='\0';
        strncpy(tmp, out, 1023);
    }
    strncpy(buffer, tmp, buf_size-1); buffer[buf_size-1]='\0';
    return buffer;
}

static void extract_styles(dom_node *node, const char *base_url, int download_assets) {
    if (!node) return;
    if (node->type == NODE_ELEMENT) {
        if (node->tag && strcasecmp(node->tag, "style") == 0) {
            if (node->child_count > 0 && node->children[0]->type == NODE_TEXT) parse_css_text(node->children[0]->text);
        } else if (node->tag && strcasecmp(node->tag, "link") == 0) {
            const char *rel  = get_attribute(node, "rel");
            const char *href = get_attribute(node, "href");
            if (rel && strcasecmp(rel, "stylesheet") == 0 && href) {
                char target_url[8192] = {0};
                if (strncmp(href,"http://",7)==0||strncmp(href,"https://",8)==0) strncpy(target_url, href, 8191);
                    else if (strncmp(href,"//",2)==0) snprintf(target_url, 8192, "https:%s", href);
                        else {
                            char base_copy[8192]; strncpy(base_copy, base_url, 8191);
                            char *sl = strrchr(base_copy,'/');
                            if (sl && sl > strchr(base_copy,':')+2) *(sl+1)='\0';
                            else if (!sl) strcat(base_copy,"/");
                            snprintf(target_url, 8192, "%s%s", base_copy, href);
                        }

                        char hostname[8192]={0}, path[8192]={0}; const char *port="80";
                    const char *us = target_url;
                    if (strncmp(us,"http://",7)==0) us+=7; else if (strncmp(us,"https://",8)==0) { us+=8; port="443"; }
                        const char *sl = strchr(us,'/');
                        if (sl) { strncpy(hostname, us, sl-us); strcpy(path, sl); } else { strcpy(hostname, us); strcpy(path, "/"); }
                        char *q = strchr(hostname,'?'); if(q) *q='\0';

                        printf("fetching external css: %s...\n", target_url);
                        size_t css_size=0;
                        char *css_data = fetch_html(hostname, port, path, &css_size);
                        if (css_data) {
                            char *body = strstr(css_data, "\r\n\r\n");
                            if (body) {
                                body += 4;
                                parse_css_text(body);
                            }
                            free(css_data);
                        }
            }
        }
    }
    for (int i=0; i<node->child_count; i++) extract_styles(node->children[i], base_url, download_assets);
}

static int rule_cmp(const void *a, const void *b) {
    const css_rule_t *ra = (const css_rule_t*)a;
    const css_rule_t *rb = (const css_rule_t*)b;
    if (ra->specificity != rb->specificity) return ra->specificity - rb->specificity;
    return ra->order - rb->order;
}

static void apply_styles_to_node(dom_node *node) {
    if (!node || node->type != NODE_ELEMENT) return;
    for (int i=0; i<rule_count; i++) {
        if (matches_selector(node, rules[i].selector)) {
            char resolved[1024];
            set_style(node, rules[i].prop, resolve_var(rules[i].val, resolved, sizeof(resolved)));
        }
    }
    for (int i=0; i<node->child_count; i++) apply_styles_to_node(node->children[i]);
}

void process_css(dom_node *root, const char *base_url, int download_assets) {
    for (int i=0; i<rule_count; i++) { free(rules[i].selector); free(rules[i].prop); free(rules[i].val); }
    rule_count = 0; rule_order = 0;
    for (int i=0; i<css_var_count; i++) { free(css_vars[i].name); free(css_vars[i].val); }
    css_var_count = 0;

    printf("extracting stylesheets...\n");
    extract_styles(root, base_url, download_assets);
    if (rule_count > 1) qsort(rules, rule_count, sizeof(css_rule_t), rule_cmp);
    printf("applying %d css rules...\n", rule_count);
    apply_styles_to_node(root);
}
