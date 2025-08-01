#include "../engine/board.cpp"
#include <thread>
#include <atomic>

namespace process1 {
    #include "../engine/bot.cpp"
}
namespace process2 {
    #include "../engine/bot.cpp"
}


// Each position is played twice
int POSITIONS_COUNT;
float TOTAL_PLAYED;
const int MAX_MOVE_COUNT = 300;

std::atomic<int> AWins (0);  // TEST_VAR = false
std::atomic<int> BWins (0);  // TEST_VAR = true
std::atomic<int> draws (0);


void doTests1(bool isAWhite, Board * boards) {
    for (int i = 0 ; i < POSITIONS_COUNT ; i++) {
        if (isAWhite) {
            printf("Game n°%d, A white\n", i+1);
        } else {
            printf("Game n°%d, B white\n", i+1);
        }
        
        Board board = boards[i].copy();

        int moveCount = 0;
        while (board.state == NEUTRAL && moveCount < MAX_MOVE_COUNT) {
            if (isAWhite == board.whiteTurn) {
                process1::TEST_VAR = false;
            } else {
                process1::TEST_VAR = true;
            }

            auto bestResult = process1::getBestMove(board, false);

            board.playMove(bestResult.move);
            process1::onMovePlayed(board);
            moveCount += 1;
        }

        if (moveCount >= MAX_MOVE_COUNT) {
            printf("Max move count exceeded\n");
        }

        // printBoard(board);

        if (board.state == WHITE_WON) {
            if (isAWhite) {
                printf("A wins\n");
                AWins += 1;
            } else {
                printf("B wins\n");
                BWins += 1;
            }
        } else if (board.state == BLACK_WON) {
            if (isAWhite) {
                printf("B wins\n");
                BWins += 1;
            } else {
                printf("A wins\n");
                AWins += 1;
            }
        } else {
            draws += 1;
        }
    }
}

void doTests2(bool isAWhite, Board * boards) {
    for (int i = 0 ; i < POSITIONS_COUNT ; i++) {
        if (isAWhite) {
            printf("Game n°%d, A white\n", i+1);
        } else {
            printf("Game n°%d, B white\n", i+1);
        }
        
        Board board = boards[i].copy();

        int moveCount = 0;
        while (board.state == NEUTRAL && moveCount < MAX_MOVE_COUNT) {
            if (isAWhite == board.whiteTurn) {
                process2::TEST_VAR = false;
            } else {
                process2::TEST_VAR = true;
            }

            auto bestResult = process2::getBestMove(board, false);

            board.playMove(bestResult.move);
            process2::onMovePlayed(board);
            moveCount += 1;
        }

        if (moveCount >= MAX_MOVE_COUNT) {
            printf("Max move count exceeded\n");
        }

        // printBoard(board);

        if (board.state == WHITE_WON) {
            if (isAWhite) {
                printf("A wins\n");
                AWins += 1;
            } else {
                printf("B wins\n");
                BWins += 1;
            }
        } else if (board.state == BLACK_WON) {
            if (isAWhite) {
                printf("B wins\n");
                BWins += 1;
            } else {
                printf("A wins\n");
                AWins += 1;
            }
        } else {
            draws += 1;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        POSITIONS_COUNT = atoi(argv[1]);
    } else {
        POSITIONS_COUNT = 20; // 100;
    }

    printf("Testing %dx2 Positions...\n", POSITIONS_COUNT);


    
    TOTAL_PLAYED = POSITIONS_COUNT * 2.0f;

    genBitboardConstants();
    genZobristKeys();
    process1::initBot();
    process2::initBot();

    std::ifstream positionsFile ("testing/positions.bin", std::ios_base::binary);

    Board boards[POSITIONS_COUNT];
    for (int i = 0 ; i < POSITIONS_COUNT ; i++) {
        boards[i] = loadBoardFile(positionsFile, i);
    }
    positionsFile.close();

    std::thread thread1 (doTests1, false, (Board *) boards);
    std::thread thread2 (doTests2, true, (Board *) boards);

    thread1.join();
    thread2.join();

    printf("\n --- Results --- \n");
    printf("| Over %dx2 games played :\n", POSITIONS_COUNT);
    printf("| %d won by A (%f %)", AWins.load(), 100.0f * AWins.load()/TOTAL_PLAYED);
    printf("| %d won by B (%f %)", BWins.load(), 100.0f * BWins.load()/TOTAL_PLAYED);
    printf("| %d draws (%f %)", draws.load(), 100.0f * draws.load()/TOTAL_PLAYED);

    return EXIT_SUCCESS;
}
