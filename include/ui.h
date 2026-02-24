#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>

#define MAX_TABS 5
#define MAX_URL_LEN 8192

typedef struct {
    char url[MAX_URL_LEN];
    char title[128];
    int scroll_y;
    void *tree;
} browser_tab;

typedef struct {
    browser_tab tabs[MAX_TABS];
    int tab_count;
    int active_tab;
    char input_buffer[MAX_URL_LEN];
    int is_focused;
    int is_loading;

    SDL_Rect back_btn;
    SDL_Rect fwd_btn;
    SDL_Rect url_box;
    SDL_Rect add_tab_btn;
    SDL_Rect close_btns[MAX_TABS];
} browser_ui;

void init_ui(browser_ui *ui);
void draw_ui(browser_ui *ui, int win_w);
int handle_ui_click(browser_ui *ui, int x, int y, int *closed_tab_index);
void ui_format_search(const char *input, char *output);

#endif
