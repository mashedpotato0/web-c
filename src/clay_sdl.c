#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_sdl.h"
#include "renderer.h"
#include <SDL2/SDL_ttf.h>

void clay_sdl_render(SDL_Renderer *renderer, Clay_RenderCommandArray commands) {
    for (int i = 0; i < commands.length; i++) {
        Clay_RenderCommand *cmd = &commands.internalArray[i];
        SDL_Rect rect = {
            (int)cmd->boundingBox.x,
            (int)cmd->boundingBox.y,
            (int)cmd->boundingBox.width,
            (int)cmd->boundingBox.height
        };

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData *config = &cmd->renderData.rectangle;
                SDL_SetRenderDrawColor(renderer, config->backgroundColor.r, config->backgroundColor.g, config->backgroundColor.b, config->backgroundColor.a);
                SDL_RenderFillRect(renderer, &rect);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderRenderData *config = &cmd->renderData.border;
                SDL_SetRenderDrawColor(renderer, config->color.r, config->color.g, config->color.b, config->color.a);
                SDL_RenderDrawRect(renderer, &rect);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData *config = &cmd->renderData.text;
                TTF_Font *font = get_font_by_id(config->fontId);
                if (!font) break;

                SDL_Color color = {config->textColor.r, config->textColor.g, config->textColor.b, config->textColor.a};

                char buf[4096];
                int len = config->stringContents.length < 4095 ? config->stringContents.length : 4095;
                memcpy(buf, config->stringContents.chars, len);
                buf[len] = '\0';

                SDL_Surface *surf = TTF_RenderUTF8_Blended(font, buf, color);
                if (surf) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dest = {rect.x, rect.y, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, NULL, &dest);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(surf);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                Clay_ImageRenderData *config = &cmd->renderData.image;
                if (config->imageData) {
                    SDL_RenderCopy(renderer, (SDL_Texture*)config->imageData, NULL, &rect);
                } else {
                    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
                    SDL_RenderFillRect(renderer, &rect);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                SDL_RenderSetClipRect(renderer, &rect);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                SDL_RenderSetClipRect(renderer, NULL);
                break;
            }
            default:
                break;
        }
    }
}
