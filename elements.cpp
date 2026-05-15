#ifndef ELEMENTS
#define ELEMENTS

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "shared.cpp"
#include "UIConstants.cpp"
#include <iostream>


class Button {
    public:
        Button() {} // Should not be used
        Button(SDL_Renderer * renderer, Shared * shared, 
               const char * imageFile, SDL_Point pos, Menu menu, 
               void (*callback) (Shared *)) {
            
            this->renderer = renderer;
            this->shared = shared;

            texture = IMG_LoadTexture(renderer, imageFile);

            int w, h;
            SDL_QueryTexture(texture, NULL, NULL, &w, &h);
            this->rect = {pos.x, pos.y, w, h};

            this->menu = menu;
            clickCallback = callback;
        }

        void update(int xMouse, int yMouse) {
            if (collidesMouse(xMouse, yMouse)) {
                shared->currentCursor = shared->CURSOR_HAND;
            }

            SDL_RenderCopy(renderer, texture, NULL, &rect);
        }

        bool collidesMouse(int xMouse, int yMouse) {
            SDL_Point point{xMouse, yMouse};
            return SDL_PointInRect(&point, &rect);
        }

        void onClick() {
            clickCallback(shared);
        }

        Menu menu;

    private:
        SDL_Renderer * renderer;
        Shared * shared;

        SDL_Texture * texture;
        SDL_Rect rect;

        void (*clickCallback) (Shared *);
};


class Text {
    public:
        Text() {} // Should not be used
        Text(SDL_Renderer * renderer, Shared * shared, 
             TTF_Font * font, SDL_Point pos, Menu menu, const char * text,
             bool hidden=false, bool isCenteredX=false, bool isCenteredY=false, bool transparentBg=false) {
            
            this->renderer = renderer;
            this->shared = shared;      
            
            this->font = font;
            this->pos = pos;
            this->rect = {};

            this->menu = menu;
            this->hidden = hidden;

            this->isCenteredX = isCenteredX;
            this->isCenteredY = isCenteredY;

            // NOT IMPLEMENTED
            this->transparentBg = transparentBg;

            setText(text);
        }

        void setText(const char * text) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }

            strcpy(this->text, text);

            SDL_Surface * textSurface;
            /*if (transparentBg) {
                textSurface = TTF_RenderUTF8_LCD(font, text, {0, 0, 0}, {255, 255, 255, 0});
            } else {*/
                textSurface = TTF_RenderUTF8_LCD(font, text, {0, 0, 0}, {255, 255, 255});
            //}
            rect.w = textSurface->w;
            rect.h = textSurface->h;
            texture = SDL_CreateTextureFromSurface(renderer, textSurface);

            SDL_FreeSurface(textSurface);
            computeRect();
        }

        void setPos(SDL_Point pos) {
            this->pos = pos;
            computeRect();
        }

        void update() {
            if (!hidden) {
                SDL_RenderCopy(renderer, texture, NULL, &rect);
            }
        }

        Menu menu;
        bool hidden;

    private:
        SDL_Renderer * renderer;
        Shared * shared;

        SDL_Texture * texture = nullptr;
        TTF_Font * font;
        SDL_Point pos;
        SDL_Rect rect;

        bool isCenteredX;
        bool isCenteredY;

        bool transparentBg;

        char text[100];

        void computeRect() {
            /*Already done
            int w, h;
            TTF_SizeUTF8(font, text, &w, &h);
            rect.w = w;
            rect.h = h;*/

            if (isCenteredX) {
                rect.x = pos.x - rect.w/2;
            } else {
                rect.x = pos.x;
            }
            if (isCenteredY) {
                rect.y = pos.y - rect.h/2;
            } else {
                rect.y = pos.y;
            }
        }
};


// Forward declaration
void settingsCallback(Shared *);
void debugToggleCallback(Shared *);
void settingsBackCallback(Shared *);


class Elements {
    public:
        Elements(SDL_Renderer * renderer, Shared * shared) {
            // Elements
            settingsButton = {renderer, shared, "assets/buttons/settings80.png", {SCREEN_WIDTH - 80 - 20, 20}, Menu::playing, &settingsCallback};
            debugToggleButton = {renderer, shared, "assets/buttons/circleButton.png", {40, 150}, Menu::settings, &debugToggleCallback};
            settingsBackButton = {renderer, shared, "assets/buttons/back.png", {20, 20}, Menu::settings, &settingsBackCallback};

            buttons[0] = &settingsButton;
            buttons[1] = &debugToggleButton;
            buttons[2] = &settingsBackButton;

            debugSettingText = {renderer, shared, shared->mediumFont, {140, 168}, Menu::settings, "Debug activé"};
            counterText = {renderer, shared, shared->smallFont, {20, 10}, Menu::playing, "Noeuds : 0", !shared->debugEnabled};
            evaluationText = {renderer, shared, shared->verySmallFont, {0, 0}, Menu::playing, "0.0", false, true, false, true};
            zobristHashText = {renderer, shared, shared->smallFont, {400, 10}, Menu::playing, " ",  !shared->debugEnabled};
            depthText = {renderer, shared, shared->smallFont, {20, 50}, Menu::playing, "Depth : ",  !shared->debugEnabled};
            
            texts[0] = &debugSettingText;
            texts[1] = &counterText;
            texts[2] = &evaluationText;
            texts[3] = &zobristHashText;
            texts[4] = &depthText;
        }

        Button settingsButton;
        Button debugToggleButton;
        Button settingsBackButton;
        Button * buttons[3];

        Text debugSettingText;
        Text counterText;
        Text evaluationText;
        Text zobristHashText;
        Text depthText;
        Text * texts[5];
};


void settingsCallback(Shared * shared) {
    shared->menu = Menu::settings;
}
void debugToggleCallback(Shared * shared) {
    shared->debugEnabled = !shared->debugEnabled;

    if (shared->debugEnabled) {
        shared->elements->debugSettingText.setText("Debug activé");
        shared->elements->counterText.hidden = false;
        shared->elements->zobristHashText.hidden = false;
        shared->elements->depthText.hidden = false;
    } else {
        shared->elements->debugSettingText.setText("Debug désactivé");
        shared->elements->counterText.hidden = true;
        shared->elements->zobristHashText.hidden = true;
        shared->elements->depthText.hidden = true;
    }
}
void settingsBackCallback(Shared * shared) {
    shared->menu = Menu::playing;
}

#endif
