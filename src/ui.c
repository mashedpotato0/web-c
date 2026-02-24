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
    decl_top.backgroundColor = (Clay_Color){235, 236, 240, 255};
    Clay__OpenElementWithId(CLAY_ID("browser_top_bar"));
    Clay__ConfigureOpenElement(decl_top);

    Clay_ElementDeclaration decl_tabs = {0};
    decl_tabs.layout.sizing.width = CLAY_SIZING_GROW();
    decl_tabs.layout.sizing.height = CLAY_SIZING_FIXED(36);
    decl_tabs.layout.padding = (Clay_Padding){8, 8, 8, 0};
    decl_tabs.layout.childGap = 4;
    decl_tabs.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
    Clay__OpenElementWithId(CLAY_ID("tabs_row"));
    Clay__ConfigureOpenElement(decl_tabs);

    for (int i = 0; i < ui->tab_count; i++) {
        int is_active = (i == ui->active_tab);
        Clay_Color tab_color = is_active ? (Clay_Color){255, 255, 255, 255} : (Clay_Color){220, 222, 227, 255};

        Clay_ElementDeclaration decl_tab = {0};
        decl_tab.layout.sizing.width = CLAY_SIZING_FIXED(160);
        decl_tab.layout.sizing.height = CLAY_SIZING_GROW();
        decl_tab.layout.padding = (Clay_Padding){10, 8, 0, 0};
        decl_tab.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        decl_tab.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
        decl_tab.backgroundColor = tab_color;
        decl_tab.cornerRadius = (Clay_CornerRadius){8, 8, 0, 0};
        Clay__OpenElementWithId(CLAY_IDI("tab", i));
        Clay__ConfigureOpenElement(decl_tab);

        Clay_ElementDeclaration decl_title = {0};
        decl_title.layout.sizing.width = CLAY_SIZING_GROW();
        decl_title.layout.sizing.height = CLAY_SIZING_GROW();
        Clay__OpenElement();
        Clay__ConfigureOpenElement(decl_title);

        static char disp_titles[MAX_TABS][32];
        strncpy(disp_titles[i], ui->tabs[i].title, 31);
        disp_titles[i][31] = '\0';
        if (strlen(ui->tabs[i].title) > 25) {
            strcpy(&disp_titles[i][22], "...");
        }

        Clay_TextElementConfig txt_cfg = { .textColor = {30, 30, 30, 255}, .fontId = 2, .fontSize = 16 };
        Clay_String str = { .chars = disp_titles[i], .length = (int)strlen(disp_titles[i]) };
        Clay__OpenTextElement(str, &txt_cfg);
        Clay__CloseElement();

        Clay_ElementDeclaration decl_close = {0};
        decl_close.layout.sizing.width = CLAY_SIZING_FIXED(20);
        decl_close.layout.sizing.height = CLAY_SIZING_FIXED(20);
        decl_close.layout.childAlignment = (Clay_ChildAlignment){CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
        decl_close.cornerRadius = (Clay_CornerRadius){10, 10, 10, 10};
        decl_close.backgroundColor = is_active ? (Clay_Color){240, 240, 240, 255} : (Clay_Color){205, 207, 212, 255};
        Clay__OpenElementWithId(CLAY_IDI("close_btn", i));
        Clay__ConfigureOpenElement(decl_close);

        Clay_TextElementConfig close_txt = { .textColor = {100, 100, 100, 255}, .fontId = 2, .fontSize = 14 };
        Clay_String c_str = { .chars = "x", .length = 1 };
        Clay__OpenTextElement(c_str, &close_txt);
        Clay__CloseElement();

        Clay__CloseElement();
    }

    Clay_ElementDeclaration decl_add = {0};
    decl_add.layout.sizing.width = CLAY_SIZING_FIXED(28);
    decl_add.layout.sizing.height = CLAY_SIZING_FIXED(28);
    decl_add.layout.childAlignment = (Clay_ChildAlignment){CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
    decl_add.cornerRadius = (Clay_CornerRadius){14, 14, 14, 14};
    decl_add.backgroundColor = (Clay_Color){210, 212, 217, 255};
    Clay__OpenElementWithId(CLAY_ID("add_tab_btn"));
    Clay__ConfigureOpenElement(decl_add);
    Clay_TextElementConfig btn_txt_cfg = { .textColor = {80, 80, 80, 255}, .fontId = 2, .fontSize = 18 };
    Clay_String add_str = { .chars = "+", .length = 1 };
    Clay__OpenTextElement(add_str, &btn_txt_cfg);
    Clay__CloseElement();

    Clay__CloseElement();

    Clay_ElementDeclaration decl_toolbar = {0};
    decl_toolbar.layout.sizing.width = CLAY_SIZING_GROW();
    decl_toolbar.layout.sizing.height = CLAY_SIZING_FIXED(44);
    decl_toolbar.layout.padding = (Clay_Padding){8, 8, 6, 6};
    decl_toolbar.layout.childGap = 8;
    decl_toolbar.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
    decl_toolbar.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
    decl_toolbar.backgroundColor = (Clay_Color){255, 255, 255, 255};
    Clay__OpenElementWithId(CLAY_ID("toolbar_row"));
    Clay__ConfigureOpenElement(decl_toolbar);

    Clay_ElementDeclaration decl_btn = {0};
    decl_btn.layout.sizing.width = CLAY_SIZING_FIXED(32);
    decl_btn.layout.sizing.height = CLAY_SIZING_FIXED(32);
    decl_btn.layout.childAlignment = (Clay_ChildAlignment){CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
    decl_btn.cornerRadius = (Clay_CornerRadius){16, 16, 16, 16};
    decl_btn.backgroundColor = (Clay_Color){240, 242, 245, 255};

    Clay_TextElementConfig t_cfg = { .textColor = {80, 80, 80, 255}, .fontId = 2, .fontSize = 18 };

    Clay__OpenElementWithId(CLAY_ID("back_btn"));
    Clay__ConfigureOpenElement(decl_btn);
    Clay_String b_str = { .chars = "<", .length = 1 };
    Clay__OpenTextElement(b_str, &t_cfg);
    Clay__CloseElement();

    Clay__OpenElementWithId(CLAY_ID("fwd_btn"));
    Clay__ConfigureOpenElement(decl_btn);
    Clay_String f_str = { .chars = ">", .length = 1 };
    Clay__OpenTextElement(f_str, &t_cfg);
    Clay__CloseElement();

    Clay_ElementDeclaration decl_url = {0};
    decl_url.layout.sizing.width = CLAY_SIZING_GROW();
    decl_url.layout.sizing.height = CLAY_SIZING_FIXED(32);
    decl_url.layout.padding = (Clay_Padding){16, 16, 4, 4};
    decl_url.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
    decl_url.backgroundColor = ui->is_focused ? (Clay_Color){255, 255, 255, 255} : (Clay_Color){240, 242, 245, 255};
    decl_url.cornerRadius = (Clay_CornerRadius){16, 16, 16, 16};
    if (ui->is_focused) {
        decl_url.border = (Clay_BorderElementConfig){ .width = {2, 2, 2, 2}, .color = {100, 150, 255, 255} };
    }

    Clay__OpenElementWithId(CLAY_ID("url_box"));
    Clay__ConfigureOpenElement(decl_url);

    if (ui->input_buffer[0] != '\0') {
        Clay_TextElementConfig txt_cfg = { .textColor = {30, 30, 30, 255}, .fontId = 2, .fontSize = 15 };
        Clay_String str = { .chars = ui->input_buffer, .length = (int)strlen(ui->input_buffer) };
        Clay__OpenTextElement(str, &txt_cfg);
    }

    Clay__CloseElement();
    Clay__CloseElement();
    Clay__CloseElement();
}

int handle_ui_click(browser_ui *ui, int x, int y, int *closed_tab_index) {
    if (y < 36) {
        for (int i = 0; i < ui->tab_count; i++) {
            if (x >= ui->close_btns[i].x && x <= ui->close_btns[i].x + ui->close_btns[i].w &&
                y >= ui->close_btns[i].y && y <= ui->close_btns[i].y + ui->close_btns[i].h) {
                *closed_tab_index = i;
            return 2;
                }
        }

        int tab_w = 160;
        for (int i = 0; i < ui->tab_count; i++) {
            int tx = 8 + (i * (tab_w + 4));
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
    } else if (y >= 36 && y <= 80) {
        if (x >= ui->url_box.x && x <= ui->url_box.x + ui->url_box.w) {
            ui->is_focused = 1;
            return 1;
        }
    }
    ui->is_focused = 0;
    return 0;
}
