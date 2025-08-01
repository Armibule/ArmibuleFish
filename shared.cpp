#ifndef SHARED
#define SHARED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


// Déclaration incomplète
class Game;
class Elements;

enum class Menu {
    playing,
    settings
};


class Shared {
    public:
        Menu menu = Menu::playing;
        bool debugEnabled = true;
        bool showPV = false;
        SDL_Cursor * currentCursor;

        // Set at runtime
        Game * game;
        Elements * elements;

        TTF_Font * smallFont;
        TTF_Font * mediumFont;
        TTF_Font * bigFont;

        Shared() {
            #if defined(_WIN32) || defined(_WIN64)
                smallFont = TTF_OpenFont("C:\\Windows\\Fonts\\micross.ttf", 26);
                mediumFont = TTF_OpenFont("C:\\Windows\\Fonts\\micross.ttf", 36);
                bigFont = TTF_OpenFont("C:\\Windows\\Fonts\\micross.ttf", 66);
            #else
                printf("Font file not implemented for this platform");
                throw;
            #endif
        }

        void update() {
            if (prevCursor != currentCursor) {
                SDL_SetCursor(currentCursor);
            }

            prevCursor = currentCursor;
            currentCursor = CURSOR_ARROW;
        }

        SDL_Cursor * const CURSOR_ARROW = SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW);
        SDL_Cursor * const CURSOR_HAND = SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
        SDL_Cursor * const CURSOR_WAIT_ARROW = SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_WAITARROW);
    private:
        SDL_Cursor * prevCursor;
};


#include "elements.cpp"
#include "game.cpp"



#endif
