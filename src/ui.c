#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "clay.h"
#include "renderer.h"

void init_ui(browser_ui *ui) {
    memset(ui, 0, sizeof(browser_ui));
    ui->tab_count = 1;
    ui->active_tab = 0;
    strncpy(ui->tabs[0].url, "about:blank", MAX_URL_LEN - 1);
    strncpy(ui->tabs[0].title, "new tab", 127);
    ui->input_buffer[0] = '\0';
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

void draw_ui(browser_ui *ui, int win_w) {
    setup_clay_measure();

    Clay_ElementDeclaration decl_top = {0};
    decl_top.layout.sizing.width = CLAY_SIZING_GROW();
    decl_top.layout.sizing.height = CLAY_SIZING_FIXED(80);
    decl_top.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
    decl_top.backgroundColor = (Clay_Color){220, 220, 225, 255};
    Clay__OpenElementWithId(CLAY_ID("browser_top_bar"));
    Clay__ConfigureOpenElement(decl_top);

    Clay_ElementDeclaration decl_row = {0};
    decl_row.layout.sizing.width = CLAY_SIZING_GROW();
    decl_row.layout.sizing.height = CLAY_SIZING_FIXED(40);
    decl_row.layout.padding = (Clay_Padding){10, 10, 5, 5};
    decl_row.layout.childGap = 5;
    Clay__OpenElementWithId(CLAY_ID("tabs_row"));
    Clay__ConfigureOpenElement(decl_row);

    for (int i = 0; i < ui->tab_count; i++) {
        Clay_Color tab_color = (i == ui->active_tab) ? (Clay_Color){250, 250, 250, 255} : (Clay_Color){190, 190, 195, 255};

        Clay_ElementDeclaration decl_tab = {0};
        decl_tab.layout.sizing.width = CLAY_SIZING_FIXED(150);
        decl_tab.layout.sizing.height = CLAY_SIZING_GROW();
        decl_tab.layout.padding = (Clay_Padding){10, 10, 5, 5};
        decl_tab.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        decl_tab.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
        decl_tab.backgroundColor = tab_color;
        Clay__OpenElementWithId(CLAY_IDI("tab", i));
        Clay__ConfigureOpenElement(decl_tab);

        Clay_ElementDeclaration decl_title = {0};
        decl_title.layout.sizing.width = CLAY_SIZING_GROW();
        decl_title.layout.sizing.height = CLAY_SIZING_GROW();
        Clay__OpenElement();
        Clay__ConfigureOpenElement(decl_title);

        Clay_TextElementConfig txt_cfg = { .textColor = {30, 30, 30, 255}, .fontId = 2, .fontSize = 16 };
        Clay_String str = { .chars = ui->tabs[i].title, .length = (int)strlen(ui->tabs[i].title) };
        Clay__OpenTextElement(str, &txt_cfg);
        Clay__CloseElement();

        Clay_ElementDeclaration decl_close = {0};
        decl_close.layout.sizing.width = CLAY_SIZING_FIXED(16);
        decl_close.layout.sizing.height = CLAY_SIZING_FIXED(16);
        decl_close.backgroundColor = (Clay_Color){220, 80, 80, 255};
        Clay__OpenElementWithId(CLAY_IDI("close_btn", i));
        Clay__ConfigureOpenElement(decl_close);
        Clay__CloseElement();

        Clay__CloseElement();
    }

    Clay_ElementDeclaration decl_add = {0};
    decl_add.layout.sizing.width = CLAY_SIZING_FIXED(30);
    decl_add.layout.sizing.height = CLAY_SIZING_FIXED(30);
    decl_add.backgroundColor = (Clay_Color){200, 200, 200, 255};
    Clay__OpenElementWithId(CLAY_ID("add_tab_btn"));
    Clay__ConfigureOpenElement(decl_add);
    Clay__CloseElement();

    Clay__CloseElement();

    Clay__OpenElementWithId(CLAY_ID("toolbar_row"));
    Clay__ConfigureOpenElement(decl_row);

    Clay_ElementDeclaration decl_btn = {0};
    decl_btn.layout.sizing.width = CLAY_SIZING_FIXED(30);
    decl_btn.layout.sizing.height = CLAY_SIZING_FIXED(30);
    decl_btn.backgroundColor = (Clay_Color){180, 180, 180, 255};

    Clay__OpenElementWithId(CLAY_ID("back_btn"));
    Clay__ConfigureOpenElement(decl_btn);
    Clay__CloseElement();

    Clay__OpenElementWithId(CLAY_ID("fwd_btn"));
    Clay__ConfigureOpenElement(decl_btn);
    Clay__CloseElement();

    Clay_ElementDeclaration decl_url = {0};
    decl_url.layout.sizing.width = CLAY_SIZING_GROW();
    decl_url.layout.sizing.height = CLAY_SIZING_FIXED(30);
    decl_url.layout.padding = (Clay_Padding){10, 10, 5, 5};
    decl_url.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
    decl_url.backgroundColor = ui->is_focused ? (Clay_Color){255, 255, 255, 255} : (Clay_Color){240, 240, 240, 255};

    Clay__OpenElementWithId(CLAY_ID("url_box"));
    Clay__ConfigureOpenElement(decl_url);

    if (ui->input_buffer[0] != '\0') {
        Clay_TextElementConfig txt_cfg = { .textColor = {30, 30, 30, 255}, .fontId = 2, .fontSize = 16 };
        Clay_String str = { .chars = ui->input_buffer, .length = (int)strlen(ui->input_buffer) };
        Clay__OpenTextElement(str, &txt_cfg);
    }

    Clay__CloseElement();
    Clay__CloseElement();
    Clay__CloseElement();
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
