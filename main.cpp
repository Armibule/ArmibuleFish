#define SDL_MAIN_HANDLED

#include <iostream>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include "shared.cpp"
#include "UIConstants.cpp"
#include "game.cpp"
#include "elements.cpp"


// Compile-time features
const bool botPlaysBlack = false;
const bool botPlaysWhite = true;

const bool IS_PROFILING = false;


void profiling() {
    Board board = {};
    for (int i = 0 ; i < 5 ; i++) {
        printf("Bot plays\n");

        MoveResult moveResult = getBestMove(board, true);

        printMove(moveResult.move);
        
        board.playMove(moveResult.move);
        onMovePlayed(board);
    }
}

Board makeTestBoard() {
    uint64_t colorBitBoards[2][PIECE_TYPE_COUNT] = {{
        buildBitboard(      // Black Pawn
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // Black Bishop
            0b00000000,
            0b00100000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // Black Knight
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // Black Rook
            0b00000000,
            0b00000000,
            0b10000000,
            0b10000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // Black Queen
            0b10000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // Black King
            0b00001000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        )
    }, {buildBitboard(      // White Pawn
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // White Bishop
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // White Knight
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000
        ), buildBitboard(   // White Rook
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b10000000,
            0b00000000,
            0b10000000,
            0b00000000
        ), buildBitboard(   // White Queen
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b10000000
        ), buildBitboard(   // White King
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00001000
        )
    }};
    char castleFlag = 0b0000;
    bool whiteTurn = true;

    return loadBoard(colorBitBoards, castleFlag, whiteTurn);
}


int main(int argc, char* argv[]) {
    genBitboardConstants();
    genZobristKeys();
    initBot();

    // genAllMagic();
    // return 0;

    if (IS_PROFILING) {
        profiling();
        return 0;
    }    

    if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
        std::cout << "error initializing SDL:\n" << SDL_GetError() << std::endl;
        return EXIT_FAILURE;
    }
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Better image - ignore the error
    SetProcessDPIAware();

    SDL_Window* window = SDL_CreateWindow(
        "Armibule Fish 🐟",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, 
        SCREEN_HEIGHT, 0
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_ACCELERATED
    );

    SDL_Surface * iconSurface = IMG_Load("assets/icon.png");
    SDL_SetWindowIcon(window, iconSurface);

    SDL_Event windowEvent;

    Shared * shared = new Shared();
    Game game {renderer, shared};
    Elements elements {renderer, shared};
    shared->game = &game;
    shared->elements = &elements;

    /*std::ifstream positionsFile ("testing/positions.bin", std::ios_base::binary);
    game.board = loadBoardFile(positionsFile, 1);
    positionsFile.close();*/
    // game.board = makeTestBoard();

    bool running {true};
    while ( running ) {
        SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );
        SDL_RenderClear( renderer );

        shared->update();
        if (shared->menu == Menu::playing) {
            game.update();
        }

        // Updates buttons
        int xMouse, yMouse;
        SDL_GetMouseState( &xMouse, &yMouse );
        for (Button * button : elements.buttons) {
            if (shared->menu == button->menu) {
                button->update(xMouse, yMouse);
            }
        }
        for (Text * text : elements.texts) {
            if (shared->menu == text->menu) {
                text->update();
            }
        }
        
        SDL_RenderPresent(renderer);

        if ( SDL_PollEvent( &windowEvent ) ) {
            switch( windowEvent.type ) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    for (Button * button : elements.buttons) {
                        if (button->menu == shared->menu && button->collidesMouse(xMouse, yMouse)) {
                            button->onClick();
                        }
                    }

                    if (shared->menu == Menu::playing) {
                        int x, y;
                        SDL_GetMouseState(&x, &y);
                        game.mouseDown(x, y);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (shared->menu == Menu::playing) {
                        int x, y;
                        SDL_GetMouseState(&x, &y);
                        game.mouseUp(x, y);
                    }
                    break;
                case SDL_KEYDOWN:
                    switch (windowEvent.key.keysym.sym)
                    {
                    case SDLK_b:
                        game.botPlays();
                        break;
                    case SDLK_u:
                        game.undoMove();
                        break;
                    case SDLK_p:
                        shared->showPV = !shared->showPV;
                        break;
                    }
            }
        }

        if (game.board.state == NEUTRAL) {
            if (botPlaysBlack && !game.board.whiteTurn) {
                game.botPlays();
            }
            if (botPlaysWhite && game.board.whiteTurn) {
                game.botPlays();
            }
        }
    };

    SDL_DestroyWindow( window );
    SDL_Quit();
 
    return EXIT_SUCCESS;
}
