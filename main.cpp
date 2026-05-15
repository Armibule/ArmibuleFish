#define SDL_MAIN_HANDLED

#include <iostream>
#include <string>
#include <windows.h>
#include "game.cpp"


// Compile-time features
const int FPS = 60;
const int FRAME_DUARTION = 1000.0f / (float) FPS;       // Duration in ms
const bool IS_PROFILING = false;


void profiling() {
    Board board = {};
    Bot * bot = new Bot();
    for (int i = 0 ; i < 5 ; i++) {
        printf("Bot plays\n");

        MoveResult moveResult = bot->getBestMove(board, true);

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
                    /*case SDLK_s:
                        // Toggles piece square table view on piece hover
                        game.pieceSquareTableViewMode = !game.pieceSquareTableViewMode;
                        break;*/
                    }
            }
        }

        shared->update();

        int newTick = SDL_GetTicks();
        SDL_Delay(std::max(FRAME_DUARTION - (newTick - lastTick), 0));
        lastTick = newTick;
    };

    SDL_DestroyWindow( window );
    SDL_Quit();
 
    return EXIT_SUCCESS;
}
