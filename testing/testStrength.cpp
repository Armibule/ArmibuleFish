#include <thread>
#include <atomic>
#include "../engine/board.cpp"
#include "../engine/gameConstants.cpp"
#include "../engine/bot.cpp"
#include <array>
#include <vector>
#include <memory>
#include <chrono>


// Each position is played twice
int POSITIONS_COUNT;
float TOTAL_PLAYED;
const int MAX_MOVE_COUNT = 300;

std::atomic<int> AWins (0);  // TEST_VAR = false
std::atomic<int> BWins (0);  // TEST_VAR = true
std::atomic<int> draws (0);

std::atomic<float> depthDiffs (0.0f);


void doTests(bool isAWhite, Board * boards, int startIndex, int boardCount) {
    Bot * whiteBot = new Bot();
    Bot * blackBot = new Bot();

    if (isAWhite) {
        blackBot->TEST_VAR = true;
    } else {
        whiteBot->TEST_VAR = true;
    }

    for (int i = startIndex ; i < startIndex + boardCount ; i++) {
        whiteBot->resetBot();
        blackBot->resetBot();

        int ADepthSum = 0;
        int BDepthSum = 0;
        int AMoves = 0;
        int BMoves = 0;

        if (isAWhite) {
            printf("Game n°%d, A white\n", i+1);
        } else {
            printf("Game n°%d, B white\n", i+1);
        }
        
        Board board = boards[i].copy();

        int moveCount = 0;
        while (board.state == NEUTRAL && moveCount < MAX_MOVE_COUNT) {
            bool whiteTurn = board.whiteTurn;
            uint64_t startZobristHash = board.zobristHash;

            MoveResult bestResult;
            if (whiteTurn) {
                bestResult = whiteBot->getBestMove(board, false);

                // Avoid excessive values
                if (whiteBot->currentDepth < 15) {
                    if (isAWhite) {
                        ADepthSum += whiteBot->currentDepth;
                        AMoves += 1;
                    } else {
                        BDepthSum += whiteBot->currentDepth;
                        BMoves += 1;
                    }
                }
            } else {
                bestResult = blackBot->getBestMove(board, false);

                if (blackBot->currentDepth < 15) {
                    if (isAWhite) {
                        BDepthSum += blackBot->currentDepth;
                        BMoves += 1;
                    } else {
                        ADepthSum += blackBot->currentDepth;
                        AMoves += 1;
                    }
                }
            }

            // Error checking
            if (startZobristHash != board.zobristHash) {
                // Probably error in play move/undo move
                printf("ERROR DETECTED : Zobrist hash changed after getBestMove\n");
                printBoard(board);
                printf("-> Game n°%d (index %d)\n", i+1, i);
                if (isAWhite) {
                    printf("-> A white\n");
                } else {
                    printf("-> B white\n");
                }
                printf("-> Start hash (%llx), End hash (%llx)\n", startZobristHash, board.zobristHash);
                std::cout.flush();
                throw;
            }

            board.playMove(bestResult.move);

            // Error checking
            if (board.state == NEUTRAL) {
                if (whiteTurn == board.whiteTurn) {
                    // Probably error in move undo
                    printf("ERROR DETECTED : Board turn did not change\n");
                    printBoard(board);
                    printf("-> Game n°%d (index %d)\n", i+1, i);
                    if (isAWhite) {
                        printf("-> A white\n");
                    } else {
                        printf("-> B white\n");
                    }
                    std::cout.flush();
                    throw;
                }
                if (bestResult.move == NO_MOVE) {
                    printf("ERROR DETECTED : Null move played\n");
                    printBoard(board);
                    printf("-> Game n°%d (index %d)\n", i+1, i);
                    if (isAWhite) {
                        printf("-> A white\n");
                    } else {
                        printf("-> B white\n");
                    }
                    if (whiteTurn) {
                        printf("-> White turn\n");

                        printf("| Principal variation (size %d):\n", whiteBot->principalVariation.size());
                        for (const Move &move : whiteBot->principalVariation) {
                            printMove(move);
                        }
                    } else {
                        printf("-> Black turn\n");

                        printf("| Principal variation (size %d):\n", blackBot->principalVariation.size());
                        for (const Move &move : blackBot->principalVariation) {
                            printMove(move);
                        }
                    }
                    std::cout.flush();
                    throw;
                }
            } 
            
            whiteBot->onMovePlayed(board);
            blackBot->onMovePlayed(board);
            moveCount += 1;
        }

        if (moveCount >= MAX_MOVE_COUNT) {
            printf("Max move count exceeded\n");
        }

        // printBoard(board);

        if (board.state == WHITE_WON) {
            if (isAWhite) {
                printf("A wins");
                AWins += 1;
            } else {
                printf("B wins");
                BWins += 1;
            }
        } else if (board.state == BLACK_WON) {
            if (isAWhite) {
                printf("B wins");
                BWins += 1;
            } else {
                printf("A wins");
                AWins += 1;
            }
        } else {
            printf("Draw");
            draws += 1;
        }

        float averageDepthA = std::round(100.0f * ((float) ADepthSum) / ((float) AMoves)) * 0.01f;
        float averageDepthB = std::round(100.0f * ((float) BDepthSum) / ((float) BMoves)) * 0.01f;

        float depthDiff = averageDepthB-averageDepthA;

        depthDiffs += depthDiff;

        printf(" - Average depths A %f, B %f, diff %f\n", averageDepthA, averageDepthB, depthDiff);
    }
}

int main(int argc, char* argv[]) {
    genBitboardConstants();
    genZobristKeys();
    initBot();

    int startIndex = 0;
    if (argc == 2) {
        POSITIONS_COUNT = atoi(argv[1]);
    } else if (argc == 3) {
        startIndex = atoi(argv[1]);
        int endIndex = atoi(argv[2]);

        POSITIONS_COUNT = endIndex - startIndex;
    } else {
        POSITIONS_COUNT = 20;
    }

    printf("Testing %dx2 Positions...\n", POSITIONS_COUNT);

    TOTAL_PLAYED = POSITIONS_COUNT * 2.0f;

    std::ifstream positionsFile ("testing/positions.bin", std::ios_base::binary);

    Board boards[POSITIONS_COUNT];
    for (int i = 0 ; i < POSITIONS_COUNT ; i++) {
        boards[i] = loadBoardFile(positionsFile, startIndex + i);
    }
    positionsFile.close();

    int firstHalfCount = POSITIONS_COUNT / 2;
    int secondHalfCount = POSITIONS_COUNT - firstHalfCount;

    // Create 4 threads to do parallel computation
    std::thread thread1 (doTests, false, (Board *) boards, 0, firstHalfCount);
    std::thread thread2 (doTests, true, (Board *) boards, 0, firstHalfCount);

    std::thread thread3 (doTests, false, (Board *) boards, firstHalfCount, secondHalfCount);
    std::thread thread4 (doTests, true, (Board *) boards, firstHalfCount, secondHalfCount);

    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();

    printf("\n --- Results --- \n");
    printf("| Over %dx2 games played :\n", POSITIONS_COUNT);
    printf("| %d Won by A (%f %)\n", AWins.load(), 100.0f * AWins.load()/TOTAL_PLAYED);
    printf("| %d Won by B (%f %)\n", BWins.load(), 100.0f * BWins.load()/TOTAL_PLAYED);
    printf("| %d Draws (%f %)\n", draws.load(), 100.0f * draws.load()/TOTAL_PLAYED);
    printf("| Average depth gain by B : %f\n", depthDiffs.load()/TOTAL_PLAYED);

    return EXIT_SUCCESS;
}
