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
             bool hidden=false) {
            
            this->renderer = renderer;
            this->shared = shared;      
            
            this->font = font;
            this->rect = {pos.x, pos.y, 0, 0};

            this->menu = menu;
            this->hidden = hidden;

            setText(text);
        }

        void setText(const char * text) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }

            SDL_Surface * textSurface = TTF_RenderUTF8_LCD(font, text, {0, 0, 0}, {255, 255, 255});
            texture = SDL_CreateTextureFromSurface(renderer, textSurface);
            
            rect.w = textSurface->w;
            rect.h = textSurface->h;

            SDL_FreeSurface(textSurface);
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
        SDL_Rect rect;
        TTF_Font * font;
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
            counterText = {renderer, shared, shared->smallFont, {20, 20}, Menu::playing, "Noeuds évalués: 0", !shared->debugEnabled};
            texts[0] = &debugSettingText;
            texts[1] = &counterText;
        }

        Button settingsButton;
        Button debugToggleButton;
        Button settingsBackButton;
        Button * buttons[3];

        Text debugSettingText;
        Text counterText;
        Text * texts[2];
};


void settingsCallback(Shared * shared) {
    shared->menu = Menu::settings;
}
void debugToggleCallback(Shared * shared) {
    shared->debugEnabled = !shared->debugEnabled;

    if (shared->debugEnabled) {
        shared->elements->debugSettingText.setText("Debug activé");
        shared->elements->counterText.hidden = false;
    } else {
        shared->elements->debugSettingText.setText("Debug désactivé");
        shared->elements->counterText.hidden = true;
    }
}
void settingsBackCallback(Shared * shared) {
    shared->menu = Menu::playing;
}

#endif
