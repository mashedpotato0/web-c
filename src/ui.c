#include <stdio.h>
#include <string.h>
#include "ui.h"

void init_ui(browser_ui *ui) {
    memset(ui, 0, sizeof(browser_ui));
    ui->tab_count = 1;
    ui->active_tab = 0;
    strncpy(ui->tabs[0].url, "about:blank", MAX_URL_LEN - 1);
    strncpy(ui->tabs[0].title, "new tab", 127);
    ui->input_buffer[0] = '\0'; /* start with empty address bar */
}

void ui_format_search(const char *input, char *output) {
    if (strchr(input, ' ') != NULL || strchr(input, '.') == NULL) {
        strcpy(output, "https://www.bing.com/search?q=");
        char *ptr = output + strlen(output);
        for (int i = 0; input[i]; i++) {
            if (input[i] == ' ') *ptr++ = '+';
            else *ptr++ = input[i];
        }
        *ptr = '\0';
    } else {
        if (strncmp(input, "http", 4) != 0) {
            snprintf(output, MAX_URL_LEN, "https://%s", input);
        } else {
            strcpy(output, input);
        }
    }
}

void draw_ui(SDL_Renderer *rend, browser_ui *ui, int win_w) {
    SDL_Rect ui_bg = {0, 0, win_w, 80};
    SDL_SetRenderDrawColor(rend, 220, 220, 225, 255);
    SDL_RenderFillRect(rend, &ui_bg);

    int tab_w = 150;
    for (int i = 0; i < ui->tab_count; i++) {
        SDL_Rect t_rect = { 10 + (i * (tab_w + 5)), 5, tab_w, 30 };
        if (i == ui->active_tab) SDL_SetRenderDrawColor(rend, 250, 250, 250, 255);
        else SDL_SetRenderDrawColor(rend, 190, 190, 195, 255);

        SDL_RenderFillRect(rend, &t_rect);
        SDL_SetRenderDrawColor(rend, 150, 150, 150, 255);
        SDL_RenderDrawRect(rend, &t_rect);

        ui->close_btns[i] = (SDL_Rect){ t_rect.x + tab_w - 22, 12, 16, 16 };
        SDL_SetRenderDrawColor(rend, 220, 80, 80, 255);
        SDL_RenderFillRect(rend, &ui->close_btns[i]);
        SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
        SDL_RenderDrawLine(rend, ui->close_btns[i].x + 4, ui->close_btns[i].y + 4, ui->close_btns[i].x + 12, ui->close_btns[i].y + 12);
        SDL_RenderDrawLine(rend, ui->close_btns[i].x + 12, ui->close_btns[i].y + 4, ui->close_btns[i].x + 4, ui->close_btns[i].y + 12);
    }

    ui->add_tab_btn = (SDL_Rect){ 10 + (ui->tab_count * (tab_w + 5)), 5, 30, 30 };
    SDL_SetRenderDrawColor(rend, 200, 200, 200, 255);
    SDL_RenderFillRect(rend, &ui->add_tab_btn);
    SDL_SetRenderDrawColor(rend, 100, 100, 100, 255);
    SDL_RenderDrawLine(rend, ui->add_tab_btn.x + 15, ui->add_tab_btn.y + 8, ui->add_tab_btn.x + 15, ui->add_tab_btn.y + 22);
    SDL_RenderDrawLine(rend, ui->add_tab_btn.x + 8, ui->add_tab_btn.y + 15, ui->add_tab_btn.x + 22, ui->add_tab_btn.y + 15);

    ui->back_btn = (SDL_Rect){ 10, 42, 30, 30 };
    ui->fwd_btn  = (SDL_Rect){ 45, 42, 30, 30 };
    SDL_SetRenderDrawColor(rend, 180, 180, 180, 255);
    SDL_RenderFillRect(rend, &ui->back_btn);
    SDL_RenderFillRect(rend, &ui->fwd_btn);
    SDL_SetRenderDrawColor(rend, 50, 50, 50, 255);
    SDL_RenderDrawLine(rend, ui->back_btn.x + 20, ui->back_btn.y + 10, ui->back_btn.x + 10, ui->back_btn.y + 15);
    SDL_RenderDrawLine(rend, ui->back_btn.x + 10, ui->back_btn.y + 15, ui->back_btn.x + 20, ui->back_btn.y + 20);
    SDL_RenderDrawLine(rend, ui->fwd_btn.x + 10, ui->fwd_btn.y + 10, ui->fwd_btn.x + 20, ui->fwd_btn.y + 15);
    SDL_RenderDrawLine(rend, ui->fwd_btn.x + 20, ui->fwd_btn.y + 15, ui->fwd_btn.x + 10, ui->fwd_btn.y + 20);

    ui->url_box = (SDL_Rect){ 85, 42, win_w - 105, 30 };
    if (ui->is_focused) SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
    else SDL_SetRenderDrawColor(rend, 240, 240, 240, 255);
    SDL_RenderFillRect(rend, &ui->url_box);
    SDL_SetRenderDrawColor(rend, 100, 100, 100, 255);
    SDL_RenderDrawRect(rend, &ui->url_box);

    if (ui->is_loading) {
        SDL_Rect load_bar = { 85, 72, win_w - 105, 3 };
        SDL_SetRenderDrawColor(rend, 50, 150, 255, 255);
        SDL_RenderFillRect(rend, &load_bar);
    }

    SDL_SetRenderDrawColor(rend, 100, 100, 100, 255);
    SDL_RenderDrawLine(rend, 0, 79, win_w, 79);
}

int handle_ui_click(browser_ui *ui, int x, int y, int *closed_tab_index) {
    if (y < 40) {
        for (int i = 0; i < ui->tab_count; i++) {
            if (x >= ui->close_btns[i].x && x <= ui->close_btns[i].x + ui->close_btns[i].w &&
                y >= ui->close_btns[i].y && y <= ui->close_btns[i].y + ui->close_btns[i].h) {
                *closed_tab_index = i;
            return 2;
                }
        }

        int tab_w = 150;
        for (int i = 0; i < ui->tab_count; i++) {
            int tx = 10 + (i * (tab_w + 5));
            if (x >= tx && x <= tx + tab_w) {
                ui->active_tab = i;
                /* sync url back into the visual input buffer */
                if (strcmp(ui->tabs[i].url, "about:blank") == 0) {
                    ui->input_buffer[0] = '\0';
                } else {
                    strncpy(ui->input_buffer, ui->tabs[i].url, MAX_URL_LEN - 1);
                }
                return 1;
            }
        }
        if (x >= ui->add_tab_btn.x && x <= ui->add_tab_btn.x + ui->add_tab_btn.w) {
            if (ui->tab_count < MAX_TABS) {
                int n = ui->tab_count;
                ui->tab_count++;
                ui->active_tab = n;
                strncpy(ui->tabs[n].url, "about:blank", MAX_URL_LEN - 1);
                strncpy(ui->tabs[n].title, "new tab", 127);
                ui->input_buffer[0] = '\0';
                return 3;
            }
        }
    } else if (y >= 42 && y <= 72) {
        if (x >= ui->url_box.x && x <= ui->url_box.x + ui->url_box.w) {
            ui->is_focused = 1;
            return 1;
        }
    }
    ui->is_focused = 0;
    return 0;
}
