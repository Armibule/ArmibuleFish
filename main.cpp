#define SDL_MAIN_HANDLED

#include <iostream>
#include <string>
#include <windows.h>
#include "game.cpp"


// Compile-time features
const int FPS = 60;
const int FPS_LOW = 30;
const int FPS_LOW2 = 20;
const int FRAME_DURATION = 1000.0f / (float) FPS;       // Duration in ms
const int FRAME_DURATION_LOW = 1000.0f / (float) FPS_LOW;       // Duration in ms, when user isn't active
const int FRAME_DURATION_LOW2 = 1000.0f / (float) FPS_LOW2;       // Duration in ms, when user isn't active

const bool IS_PROFILING = false;


void profiling() {
    DEFAULT_BOT_TIME = 1;
    MAX_BOT_TIME = 20000;
    NORMAL_DEPTH = 18;

    Board board = {};
    Bot * bot = new Bot();
    for (int i = 0 ; i < 5 ; i++) {
        printf("Bot plays\n");

        MoveResult moveResult = bot->getBestMove(board, false);

        printMove(moveResult.move);
        
        board.playMove(moveResult.move);
        bot->onMovePlayed(board);
    }
}

bool botPlaysBlack = false;
bool botPlaysWhite = false;
std::string fenString = "";


void printUsage() {
    printf(
        "\nProgram usage :\n"
        " -fen \"<FEN>\"                Loads a fen position\n"
        " -analysis \"<PGN file>\"      Special - Starts analysis mode on the pgn file\n"
        " -time <meanTime> <maxTime>  Sets think time of the engine, in milliseconds\n"
        " --white                     Plays as white\n"
        " --black                     Plays as black\n"
        " --help                      Prints this\n"
    );
    std::cout.flush();
}


void parseArguments(int argc, char * argv[]) {
    for (int i = 1 ; i < argc ; i++) {
        char * argument = argv[i];

        if (strcmp(argument, "--white") == 0) {
            botPlaysWhite = true;
        } else if (strcmp(argument, "--black") == 0) {
            botPlaysBlack = true;
        } else if (strcmp(argument, "-fen") == 0) {
            if (i + 1 >= argc) {
                printf("Missing closing \" for flag -fen\n");
                printUsage();
                exit(-1);
            }

            i += 1;
            fenString = argv[i];
        } else if (strcmp(argument, "-time") == 0) {
            if (i + 2 >= argc) {
                printf("Missing arguments for -time\n");
                printUsage();
                exit(-1);
            }

            i += 1;
            DEFAULT_BOT_TIME = std::atof(argv[i]);
            i += 1;
            MAX_BOT_TIME = std::atof(argv[i]);
        } else if (strcmp(argument, "--help") == 0) {
            printUsage();
            exit(0);
        } else {
            printUsage();
            exit(-1);
        }
    } 
}


int main(int argc, char * argv[]) {
    parseArguments(argc, argv);

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

    // Focus click is passed as an event
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    SDL_Surface * iconSurface = IMG_Load("assets/icon.png");
    SDL_SetWindowIcon(window, iconSurface);

    SDL_Event windowEvent;

    Shared * shared = new Shared();
    Bot * bot = new Bot();

    Board board;
    if (fenString.empty()) {
        board = {};
    } else {
        board = loadFEN(fenString);
    }

    Game game {renderer, shared, bot, board};
    Elements elements {renderer, shared};
    shared->game = &game;
    shared->elements = &elements;

    game.onElementsLoaded();

    // DEBUG
    /*std::ifstream positionsFile ("testPositions_10000.bin", std::ios_base::binary);
    game.board = loadBoardFile(positionsFile, 2);
    positionsFile.close();*/
    // game.board = makeTestBoard();

    int lastTick = SDL_GetTicks();

    bool running {true};
    while ( running ) {
        SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );
        SDL_RenderClear( renderer );

        shared->currentCursor = shared->CURSOR_ARROW;

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

        if (game.board.state == NEUTRAL) {
            if (botPlaysBlack && !game.board.whiteTurn) {
                game.botPlays();
            }
            if (botPlaysWhite && game.board.whiteTurn) {
                game.botPlays();
            }
        }

        while ( SDL_PollEvent( &windowEvent ) ) {
            switch( windowEvent.type ) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    shared->uiActive = 40;
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
                    shared->uiActive = 40;
                    if (shared->menu == Menu::playing) {
                        int x, y;
                        SDL_GetMouseState(&x, &y);
                        game.mouseUp(x, y);
                    }
                    break;
                case SDL_KEYDOWN:
                    shared->uiActive = 40;
                    switch (windowEvent.key.keysym.sym)
                    {
                    case SDLK_b:
                        game.botPlays();
                        break;
                    case SDLK_u:
                    case SDLK_LEFT:
                        if (game.botThread == NULL) {
                            game.undoMove();
                        }
                        
                        break;
                    case SDLK_RIGHT:
                        if (game.botThread == NULL) {
                            if (game.predictedVariation.size() > 0 && board.state == NEUTRAL)  {
                                game.playMove(game.predictedVariation[0]);
                            }
                        }
                        break;
                    case SDLK_p:
                        shared->showPV = !shared->showPV;
                        break;
                    }
            }
        }

        shared->update();

        int newTick = SDL_GetTicks();
        int elapsed = newTick - lastTick;
        
        if (shared->uiActive > 0) {
            SDL_Delay(std::max(FRAME_DURATION - elapsed, 0));
            shared->animationSpeed = 1.0f;
        } else if (shared->uiActive > -60) {
            SDL_Delay(std::max(FRAME_DURATION_LOW - elapsed, 0));
            shared->animationSpeed = 2.0f;
        } else {
            SDL_Delay(std::max(FRAME_DURATION_LOW2 - elapsed, 0));
            shared->animationSpeed = 3.0f;
        }

        lastTick = SDL_GetTicks();
    };

    SDL_DestroyWindow( window );
    SDL_Quit();
 
    return EXIT_SUCCESS;
}
