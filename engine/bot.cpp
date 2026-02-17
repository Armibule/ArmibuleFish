#include "board.cpp"
#include "botConstants.cpp"
#include <unordered_map>
#include <math.h>
#include <csignal>
#include <signal.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <stdexcept>


struct MoveResult {
    Move move = NO_MOVE;
    int score = 0;
};

enum NodeType {
    NO_NODE,    // 0 - This entry is empty
    PV_NODE,    // 1 - Best moves, exact score
    CUT_NODE,   // 2 - Move which are "too good", lower bound score (relatively)
    ALL_NODE    // 3 - Bad moves, upper bound score (relatively)
};

struct TTEntry {
    uint64_t zobristHash = 0;
    int depth = 0;      // Higher depth is better because it means it appeared higher in the tree
    Move bestMove = NO_MOVE;
    int score = 0;
    NodeType nodeType = NO_NODE;
    int creationTick = 0;    // Tick of creation
};

// For debug purposes
void printTTEntry(const TTEntry &entry) {
    printf("TTEntry(depth %d", entry.depth);
    printf(", score %d", entry.score);
    printf(", nodeType %d", entry.nodeType);
    printf(", zobristHash %llx", entry.zobristHash);
    printf(", creationTick %d)\n", entry.creationTick);
    printf(" -> Stored move: ");
    printMove(entry.bestMove);
}
void printSpaces(int depth) {
    for (int i = 0 ; i < NORMAL_DEPTH-depth ; i++) {
        printf("  ");
    }
}

bool moveResultCompareIncreasing(MoveResult &a, MoveResult &b) {
    return a.score < b.score;
}
bool moveResultCompareDecreasing(MoveResult &a, MoveResult &b) {
    return a.score > b.score;
}

/*inline int lerpScore(int openingScore, int endgameScore, GamePhase phase) {
    return (openingScore * (256 - phase) + endgameScore * phase) / 256;
}*/

#if MESURE_LEVEL >= ALL_MESURE
constexpr int bestMovesIndexes_indexesCount = 300;
#endif

// Class used for encapsulation
// Should be instantiated with new, otherwise it blows up the stack
class Bot {

public:

// Variable used to test features
bool TEST_VAR = false;

// Debug infos
#if MESURE_LEVEL >= LOW_MESURE
int nodeCount = 1;
#endif
#if MESURE_LEVEL >= ALL_MESURE
int evaluationCounter = 0;
int extensionCount = 0;
int PVHitCount = 0;
int TTCollisionCount = 0;
int TTHitCount = 0;
int NMPCount = 0;
int NMPPruneCount = 0;
int LMRCount = 0;
int LMRResearchCount = 0;
// To check if the move ordering is good
int bestMovesIndexes[bestMovesIndexes_indexesCount] = {};
int movesIndexes[bestMovesIndexes_indexesCount] = {};
#endif
int currentDepth = 0;       // Depth of the current iterative deepening iteration


Bot() {
    resetBot();
}

// Returns the relative static score of the position
// Final deth is used to find the fastest checkmate
int evaluatePosition(Board &board) {
    switch (board.state) {
    case WHITE_WON:
        if (board.whiteTurn) {
            return CHECKMATE_BASE_SCORE + initialPreviousHashesSize - board.previousHashes.size();  // Go for the fastest checkmate + avoid loops
        } else {
            return -CHECKMATE_BASE_SCORE - initialPreviousHashesSize + board.previousHashes.size();
        }
    case BLACK_WON:
        if (board.whiteTurn) {
            return -CHECKMATE_BASE_SCORE - initialPreviousHashesSize + board.previousHashes.size();
        } else {
            return CHECKMATE_BASE_SCORE + initialPreviousHashesSize - board.previousHashes.size();
        }
    case DRAW:
        return 0;
    }

    if (board.repetitionCount > currentRepetitionCount) {
        return 0;
    }

    #if MESURE_LEVEL >= ALL_MESURE
    evaluationCounter += 1;
    #endif

    int outputBucketIndex = NNUE::outputBucketIndex(board.allOccupancy);

    /*int score;
    if (board.whiteTurn) {
        score = nnue->feedForward(board.whiteAccumulator, outputBucketIndex);
    } else {
        score = nnue->feedForward(board.blackAccumulator, outputBucketIndex);
    }*/

    int score;
    if (board.whiteTurn) {
        score = nnue->feedForward(board.whiteAccumulator, board.blackAccumulator, outputBucketIndex);
    } else {
        score = nnue->feedForward(board.blackAccumulator, board.whiteAccumulator, outputBucketIndex);
    }

    // TODO : Test if it improves ? -> doesn't look that good
    if (board.isInCheck(board.whiteTurn)) {
        score -= 50; // checkValue;
    }

    // --- BOT PLAYSTYLE VARIANT TEST ---
    // To make the bot love pawns
    /*int absoluteBonus = 300 * (std::popcount(board.colorBB[WHITE][PAWN]) - std::popcount(board.colorBB[BLACK][PAWN]));
    if (board.whiteTurn) {
        score += absoluteBonus;
    } else {
        score -= absoluteBonus;
    }*/

    return score;
}

// Transposition table
int halfMoveTick = 0;

TTEntry transpositionTable[TTSize];

inline int getTTIndex(const Board &board) {
    return board.zobristHash & TTMask;
}
inline int relativeTTDepth(const TTEntry &entry) {
    return entry.depth - (halfMoveTick - entry.creationTick);
}
void updateTT(const Board &board, int depth, const Move &bestMove, int score, NodeType nodeType) {
    TTEntry * currentEntry = &transpositionTable[getTTIndex(board)];

    /*if (currentEntry->nodeType != NO_NODE) {
        TTCollisionCount += 1;
    }*/

    int relativeDepth = relativeTTDepth(*currentEntry);

    if (currentEntry->nodeType == NO_NODE || 
        relativeDepth <= depth /*||
        // If it makes the bounds tighter
        (
            relativeDepth == depth &&
            (
                (nodeType==CUT_NODE && currentEntry->nodeType == CUT_NODE && score > currentEntry->score) || 
                (nodeType==ALL_NODE && currentEntry->nodeType == ALL_NODE && score < currentEntry->score)
            )
        )*/) {
        currentEntry->zobristHash = board.zobristHash;
        currentEntry->depth = depth;
        currentEntry->bestMove = bestMove;
        currentEntry->score = score;
        currentEntry->nodeType = nodeType;
        currentEntry->creationTick = halfMoveTick;
    }
}
// Use with PV nodes
void updateTT_PV(const Board &board, int depth, const Move &bestMove, int score) {
    transpositionTable[getTTIndex(board)] = {board.zobristHash, depth, bestMove, score, PV_NODE, halfMoveTick};
}

// NOT SURE THIS DOES REALLY IMPROVE... QUITE BAD ACTUALLY

// Move history (TODO : not finished + to test)
// Indexed by moveHistory[isWhite][startSquare][endSquare]
// int moveHistory[2][64][64] = {};


std::vector<Move> principalVariation = {};


// TODO : ADAPT TO NEGAMAX
int quiescenceSearch(Board& board, int alpha, int beta, int depth=MAX_QUIESCENCE_DEPTH) {    
    int bestScore = evaluatePosition(board);

    if (depth == 0) {
        return bestScore;
    }

    #if MESURE_LEVEL >= ALL_MESURE
    extensionCount += 1;
    #endif
    
    if (bestScore >= beta) {
        return bestScore;
    }
    if (bestScore > alpha) {
        alpha = bestScore;
    }

    // Faster and better by only generating capture moves
    std::vector<Move> moves = {};

    board.getAllCaptureMoves(moves);

    const int moveCount = moves.size();

    if (moveCount == 0) {
        return bestScore;
    }

    // Move ordering + preparation
    MoveResult moveBaseEvaluations[moveCount] = {};

    for (int i = 0 ; i < moveCount ; i++) {
        Move move = moves[i];

        // This value is not a normal evaluation !
        int value = piecesStandardValue[board.getAt(move.endSquare).type] - piecesStandardValue[board.getAt(move.startSquare).type];

        moveBaseEvaluations[i] = {move, value};
    }

    // The value used is not color dependant
    std::sort(moveBaseEvaluations, moveBaseEvaluations + moveCount, moveResultCompareDecreasing);

    for (int i = 0 ; i < moveCount ; i++) {
        MoveResult baseMoveResult = moveBaseEvaluations[i];

        Move move = baseMoveResult.move;

        // TODO : DELTA PRUNING TEST (does not improve)
        /*if (TEST_VAR) {
            int takenPieceValue = piecesStandardValue[board.getAt(move.endSquare).type];
            if (bestScore + takenPieceValue + DELTA_PRUNING_MARGIN + 500 < alpha) {
                continue;
            }
        }*/

        #if MESURE_LEVEL >= LOW_MESURE
        nodeCount += 1;
        #endif

        UnmakeMoveInfo info = board.playMove(move);
        int score = -quiescenceSearch(board, -beta, -alpha, depth - 1);
        board.undoMove(move, info);

        if (score >= beta) {
            return score;
        }
        if (score > bestScore) {
            bestScore = score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return bestScore;
}


// Seems currently not effective... 
void makeSearchExtensions(Board &board, bool isInCheck, int &depth, int &remainingSearchExtensions, std::vector<Move> &moves) {
    if (remainingSearchExtensions <= 0) {
        return;
    }
    
    if (depth == 1) {
        if (moves.size() == 1) {
            depth += 1;
            remainingSearchExtensions -= 1;
            return;
        }
        // Extends if in check
        if (isInCheck) {
            depth += 1;
            remainingSearchExtensions -= 1;
            return;
        }
    }
}

int PVSearch(Board &board, int depth=NORMAL_DEPTH, int alpha=-INFINITE_SCORE, int beta=INFINITE_SCORE, int remainingSearchExtensions=MAX_SEARCH_EXTENSION) {
    if (isSearchCanceled) {
        // Worst score possible for the pervious layer of minmax
        // So this move will never be played
        return INFINITE_SCORE;
    }

    /*if (depth >= currentDepth-2) {
        printSpaces(depth);
        printf("%d  alpha %d, beta %d   start (%llx)\n", depth, alpha, beta, board.zobristHash); 
    }*/

    bool whiteTurn = board.whiteTurn;
    Move bestMove = NO_MOVE;
    bool isPV = false;

    #if MESURE_LEVEL >= LOW_MESURE
    nodeCount += 1;
    #endif

    // The handling doesn't seem right
    if (board.state != NEUTRAL || board.repetitionCount > currentRepetitionCount) {
        int score = evaluatePosition(board);
        // This is a leaf node, end of the game
        updateTT(board, depth, bestMove, score, ALL_NODE);
        return score;
    }

    // Check if the position is present in the transposition table
    Move refutationMove = NO_MOVE;
    // bool isRefutationMoveCapture = false;
    int baseScore;
    TTEntry currentEntry = transpositionTable[getTTIndex(board)];
    if (currentEntry.nodeType != NO_NODE && currentEntry.zobristHash == board.zobristHash) {
        if (relativeTTDepth(currentEntry) >= depth) {
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  IS STORED\n", depth, alpha, beta); 
            }*/

            #if MESURE_LEVEL >= ALL_MESURE
            TTHitCount += 1;
            #endif

            switch (currentEntry.nodeType) {
            case PV_NODE:
                // The score is exact
                return currentEntry.score;
            case CUT_NODE:
                // Score is lower bound (relatively)
                if (currentEntry.score >= beta) {
                    // Fail high
                    return currentEntry.score;
                }
                if (currentEntry.score > alpha) {
                    alpha = currentEntry.score-1;
                }
                break;
            case ALL_NODE:
                // Score is upper bound (relatively)
                if (currentEntry.score <= alpha) {
                    // Fail low
                    return currentEntry.score;
                }
                if (currentEntry.score < beta) {
                    beta = currentEntry.score+1;
                }
                break;
            }

            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("  -> alpha %d, beta %d\n", alpha, beta); 
            }*/
        }

        isPV = currentEntry.nodeType == PV_NODE;
        
        refutationMove = currentEntry.bestMove;
        baseScore = currentEntry.score;

        // isRefutationMoveCapture = board.isCapture(refutationMove);
    } else {
        baseScore = evaluatePosition(board);
    }

    bool isInCheck = board.isInCheck(whiteTurn);

    // Null Move Pruning, should always be before move generation
    if (depth < currentDepth-1 && 
        depth > NullMovePruningReduction && 
        // board.phase < ENDGAME_THRESHOLD &&     TODO : TO REPLACE
        std::popcount(board.allOccupancy) > 9 &&
        !isPV &&
        // Try only if the position looks good enought
        (baseScore + NMPRejectMargin >= beta) &&
        !isInCheck) {

        #if MESURE_LEVEL >= ALL_MESURE
        NMPCount += 1;
        #endif
        
        board.playNullMove();
        int nullSearchScore = -PVSearch(board, depth - NullMovePruningReduction, -beta, -beta + 1, remainingSearchExtensions);
        board.undoNullMove();

        if (nullSearchScore >= beta) {
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  NMP %d, alpha %d, beta %d\n", depth, nullSearchScore, alpha, beta); 
            }*/

            #if MESURE_LEVEL >= ALL_MESURE
            NMPPruneCount += 1;
            #endif
            
            return nullSearchScore;     // Fail high
        }
    }

    std::vector<Move> moves;
    board.getAllMoves(moves);

    const int moveCount = moves.size();

    if (moveCount == 0) {
        // This is a leaf node
        updateTT(board, depth, bestMove, baseScore, ALL_NODE);
        return baseScore;
    }

    // TODO : TEST SEARCH EXTENSIONS !
    makeSearchExtensions(board, isInCheck, depth, remainingSearchExtensions, moves);

    if (depth > 1) {
        // Move ordering
        MoveResult moveEvaluations[moveCount] = {};

        int refutationMoveBonus = 2000;
        int pvNodeBonus = 4000;
        int cutNodeBonus = 250;

        for (int i = 0 ; i < moveCount ; i++) {
            int value;

            if (moves[i] == refutationMove) {
                value = currentEntry.score + refutationMoveBonus;
            } else {
                UnmakeMoveInfo info = board.playMove(moves[i]);

                TTEntry entry = transpositionTable[getTTIndex(board)];
                if (entry.nodeType != NO_NODE && entry.zobristHash == board.zobristHash) {
                    value = entry.score;

                    if (entry.nodeType == CUT_NODE) {
                        // Put cut nodes on top
                        value += cutNodeBonus;
                    } else if (entry.nodeType == PV_NODE) {
                        // Put pv nodes first
                        value += pvNodeBonus;
                        #if MESURE_LEVEL >= ALL_MESURE
                        PVHitCount += 1;
                        #endif
                    }
                } else {
                    value = -evaluatePosition(board); //+ moveHistory[whiteTurn][moves[i].startSquare][moves[i].endSquare];
                }

                // TODO : test if it improves
                // Attacks other pieces
                uint64_t opponentPieces = board.occupencies[!whiteTurn];
                Square endSquare = moves[i].endSquare;
                if (board.attacksMask(endSquare, board.pieces[endSquare].type, whiteTurn) & opponentPieces) {
                    value += 25;
                }
                /*if (board.isInCheck(!whiteTurn)) {
                    value += 50;
                }*/

                board.undoMove(moves[i], info);

                // TODO : test if it improves
                if (board.isCapture(moves[i])) {
                    value += 50;
                }
            }

            moveEvaluations[i] = {moves[i], value};
        }

        std::sort(moveEvaluations, moveEvaluations + moveCount, moveResultCompareDecreasing);

        for (int i = 0 ; i < moveCount ; i++) {
            moves[i] = moveEvaluations[i].move;
        }
    }

    int LMRLevel = 0;       // Is increased during search
    int score;

    if (depth == 1) {
        bool futilityPruningEnabled = !isInCheck;
        // Frontier node
        for (const Move &move : moves) {
            UnmakeMoveInfo info = board.playMove(move);

            // Futility pruning, to improve
            if (futilityPruningEnabled && 
                !board.isCapture(move) && 
                (baseScore + FUTILITY_MARGIN < alpha) && 
                !board.isInCheck(!whiteTurn)) {
                board.undoMove(move, info);
                continue;
            }

            score = -quiescenceSearch(board, -beta, -alpha);
            board.undoMove(move, info);

            if (score >= beta) {
                /*moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::min(
                    moveHistory[whiteTurn][move.startSquare][move.endSquare]+depth*moveHistoryValueFactor, 
                    moveHistoryMaxValue
                );*/

                // Fail high, Cut node
                updateTT(board, depth, move, score, CUT_NODE);
                return score;
            }
            if (score > alpha) {
                bestMove = move;
                alpha = score;
            }
        }
    } else {
        /*if (depth >= currentDepth-2) {
            printSpaces(depth);
            printf("%d Start Search   (%llx)\n", depth, board.zobristHash); 
        }*/

        #if MESURE_LEVEL >= ALL_MESURE
        int bestMovesIndex = 0;
        #endif

        // Initial full search of expected best move
        Move firstMove = moves[0];
        // Fallback move in case none increase alpha (usefull in iterative deepening)
        bestMove = firstMove;

        UnmakeMoveInfo info = board.playMove(firstMove);

        score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions);        
        board.undoMove(firstMove, info);

        #if MESURE_LEVEL >= ALL_MESURE
        for (int i = 0 ; i < moveCount ; i++) {
            movesIndexes[i] += 1;
        }
        #endif

        if (score >= beta) {
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d Cut %d beta %d   (%llx)\n", depth, score, beta, board.zobristHash); 
            }*/

            #if MESURE_LEVEL >= ALL_MESURE
            bestMovesIndexes[bestMovesIndex] += 1;
            #endif

            /*moveHistory[whiteTurn][bestMove.startSquare][bestMove.endSquare] = std::min(
                moveHistory[whiteTurn][bestMove.startSquare][bestMove.endSquare]+depth*moveHistoryValueFactor, 
                moveHistoryMaxValue
            );*/

            updateTT(board, depth, firstMove, score, CUT_NODE);     // Fail high, Cut node
            return score;
        }
        if (score > alpha) {
            alpha = score;

            /*if (depth >= currentDepth-2) {
            printSpaces(depth);
            printf("%d  alpha %d, beta %d  FIRST\n", depth, alpha, beta); 
            }*/
        }

        // Starts from the second move, the first is already processed
        for (int moveIndex = 1 ; moveIndex < moveCount ; moveIndex++) {
            int nodeDepth = depth - 1;

            /*if (// depth >= minLMRDepth &&
                nodeDepth > LMRLevel + 2 &&
                LMRLevel < 2 && 
                 // depth <= currentDepth - minLMRDraft && 
                moveIndex >= LMR_MOVE_NUMBER) {
                LMRLevel += 2;// 1;
            }*/

            if (nodeDepth > LMRLevel + 1) {
                switch (LMRLevel) {
                case 0:
                    if (moveIndex >= LMR1_MOVE_NUMBER) {
                        LMRLevel = 1;
                    }
                    break;
                case 1:
                    if (moveIndex >= LMR2_MOVE_NUMBER) {
                        LMRLevel = 2;
                    }
                    break;
                case 2:
                    if (moveIndex >= LMR3_MOVE_NUMBER) {
                        LMRLevel = 3;
                    }
                    break;
                case 3:
                    if (moveIndex >= LMR4_MOVE_NUMBER) {
                        LMRLevel = 4;
                    }
                    break;
                /*case 4:
                    if (moveIndex >= LMR5_MOVE_NUMBER) {
                        LMRLevel = 5;
                    }
                    break;*/
                }
            }

            Move move = moves[moveIndex];
            UnmakeMoveInfo info = board.playMove(move);

            if (board.state != NEUTRAL || board.repetitionCount > currentRepetitionCount) {
                score = -evaluatePosition(board);
            } else {
                // Reduce less when in PV node / when giving check / when in check
                if (isInCheck || isPV /*|| board.isInCheck(board.whiteTurn)*/) {
                    // nodeDepth -= LMRLevel / 2;
                    nodeDepth -= std::max(LMRLevel - 1, 0);
                } /*else if (isRefutationMoveCapture) {
                    // TODO : Is this condition better ? NO
                    // nodeDepth = std::max(nodeDepth - LMRLevel - 1, 1);
                }*/ else {
                    nodeDepth -= LMRLevel;
                }
                // nodeDepth -= LMRLevel;

                #if MESURE_LEVEL >= ALL_MESURE
                if (LMRLevel > 0) {
                    LMRCount += 1;
                }
                #endif
                
                // Test on null window if score > alpha
                
                score = -PVSearch(board, nodeDepth, -alpha-1, -alpha, remainingSearchExtensions);
                
                // If score is within the window and windows is not null
                // Do a full research without reduction
                if (alpha < score && beta - alpha > 1) {
                    /*if (depth >= currentDepth-2) {
                        printSpaces(depth);
                        printf("RESEARCH\n");
                    }*/
                    
                    nodeDepth = depth - 1;

                    score = -PVSearch(board, nodeDepth, -beta, -alpha, remainingSearchExtensions);
                }
            }

            board.undoMove(move, info);

            if (score >= beta) {
                /*if (depth >= currentDepth-2) {
                    printSpaces(depth);
                    printf("%d Cut %d beta %d   (%llx)\n", depth, score, beta, board.zobristHash); 
                }*/

                #if MESURE_LEVEL >= ALL_MESURE
                bestMovesIndexes[bestMovesIndex] += 1;
                #endif

                /*moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::min(
                    moveHistory[whiteTurn][move.startSquare][move.endSquare]+depth*moveHistoryValueFactor, 
                    moveHistoryMaxValue
                );*/

                // Fail high, Cut node
                updateTT(board, depth, move, score, CUT_NODE); 
                return score;
            }
            if (score > alpha) {
                bestMove = move;
                alpha = score;

                #if MESURE_LEVEL >= ALL_MESURE
                bestMovesIndex = moveIndex;
                #endif

                /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  alpha %d, beta %d\n", depth, alpha, beta); 
                }*/
            } /*else {
                moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::max(
                    moveHistory[whiteTurn][move.startSquare][move.endSquare]-depth*moveHistoryValueFactor, 
                    moveHistoryMinValue
                );
            }*/
        }

        #if MESURE_LEVEL >= ALL_MESURE
        bestMovesIndexes[bestMovesIndex] += 1;
        #endif
    }

    // Debug tests
    /*if (bestMove == NO_MOVE && depth >= 5) {
        printSpaces(depth);
        printf("! NO MOVE\n! alpha %d, beta %d, depth %d\n", alpha, beta, depth);
        //throw;
    }*/
    /*if (alpha > beta) {
        printf("! ALPHA > BETA\n! alpha %d, beta %d, depth %d\n", alpha, beta, depth);
        throw;
    }*/
    /*if (bestMove != NO_MOVE) {    
    UnmakeMoveInfo info = board.playMove(bestMove);
    TTEntry ttEntry = transpositionTable[getTTIndex(board)];
    if (ttEntry.zobristHash == board.zobristHash && ttEntry.nodeType == NO_NODE && board.state == NEUTRAL) {
        printf("! NO NODE IN ALL NODE\nalpha %d, beta %d, depth %d\n", alpha, beta, depth);
        printf("Entry :\n");
        printTTEntry(ttEntry);
        printBoard(board);
        throw;
    }
    if (ttEntry.zobristHash == board.zobristHash && ttEntry.nodeType == CUT_NODE) {
        printf("! CUT NODE IN ALL NODE\nalpha %d, beta %d, depth %d\n", alpha, beta, depth);
        printf("Entry :\n");
        printTTEntry(ttEntry);
        printBoard(board);
        throw;
    }
    board.undoMove(bestMove, info);
    }*/

    /*if (depth >= currentDepth-2) { 
        printSpaces(depth);
        printf("%d TT set %d  beta %d   (%llx)", depth, alpha, beta, board.zobristHash); 
        if (bestMove == NO_MOVE) {
            printf(" NO MOVE");     // shouldn't be possible
        }
        printf("\n");
    }*/

    // Alpha acts as the best score
    updateTT(board, depth, bestMove, alpha, ALL_NODE);
    return alpha;
}


// Returns the best move with the absolute score
MoveResult getBestMove(Board &board, bool verbose=true, bool showBoard=false, bool uciInfos=false) {
    isVerbose = verbose;
    isSearchCanceled = false;
    initialPreviousHashesSize = board.previousHashes.size();

    scheduleSearchTimer(std::chrono::milliseconds((int) MAX_BOT_TIME));

    currentDepth = NORMAL_DEPTH;
    currentRepetitionCount = board.repetitionCount;

    // TESTING
    /*if (TEST_VAR) {
        LMR_MOVE_NUMBER = 2;
    } else {
        LMR_MOVE_NUMBER = 3;
    }*/
    // TEST_VAR = !board.whiteTurn;

    auto startTime = std::chrono::system_clock::now();
    auto endTime = std::chrono::system_clock::now();
    
    float elapsedTime = 0.0f;

    MoveResult bestResult = {};
    MoveResult lastBestResult = {};

    while (elapsedTime*2.0f < DEFAULT_BOT_TIME && !isSearchCanceled) {
        if (verbose) {
            printf("- Depth = %d\n", currentDepth);
        }
        if (uciInfos) {
            printf("info depth %d\n", currentDepth);
        }

        // Decays move history
        /*for (int color = 0 ; color < 2 ; color++) {
            for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
                for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                    moveHistory[color][startSquare][endSquare] = (moveHistory[color][startSquare][endSquare]*moveHistoryDecayFactor) / 256;
                }
            }
        }*/

        // Reset debug variables
        #if MESURE_LEVEL >= LOW_MESURE
        nodeCount = 1;
        #endif
        #if MESURE_LEVEL >= ALL_MESURE
        evaluationCounter = 0;
        extensionCount = 0;
        PVHitCount = 0;
        TTCollisionCount = 0;
        TTHitCount = 0;
        NMPCount = 0;
        NMPPruneCount = 0;
        LMRCount = 0;
        LMRResearchCount = 0;
        for (int i = 0 ; i < bestMovesIndexes_indexesCount ; i++) {
            bestMovesIndexes[i] = 0;
        }
        for (int i = 0 ; i < bestMovesIndexes_indexesCount ; i++) {
            movesIndexes[i] = 0;
        }
        #endif

        lastBestResult = bestResult;
        PVSearch(board, currentDepth);

        TTEntry rootEntry = transpositionTable[getTTIndex(board)];
        bestResult = {rootEntry.bestMove, rootEntry.score};

        if (bestResult.move == NO_MOVE) {
            // Skips and got directly to new iteration

            /*if (isVerbose) {
                printf("NULL MOVE !\nEntry: ");
                printTTEntry(rootEntry);
                std::cout.flush();
            }
            throw std::runtime_error("The best move is a null move !");*/
        } else {
            // Partially searched move isn't currently reliable enought
            if ((isSearchCanceled && lastBestResult.move != NO_MOVE) || bestResult.move == NO_MOVE) {
                // If this is the first iteration, we don't have a previous move
                bestResult = lastBestResult;
            }

            // Stores all PV nodes
            principalVariation.clear();

            Board pvBoard = board.copy();
            int pvDepth = currentDepth;
            int pvScore = bestResult.score;

            Move pvMove = bestResult.move;
            while (pvDepth > 1) {
                updateTT_PV(pvBoard, pvDepth, pvMove, pvScore);
                if (pvMove == NO_MOVE) {
                    // printf("No Move !\n");
                    break;
                }
                principalVariation.push_back(pvMove);

                pvDepth -= 1;
                pvBoard.playMove(pvMove);
                if (showBoard) {
                    printBoard(pvBoard);
                    printf("  ----\n");
                }

                TTEntry * entry = &transpositionTable[getTTIndex(pvBoard)];
                if (/*entry->nodeType == ALL_NODE &&*/ entry->zobristHash == pvBoard.zobristHash) {
                    //printf("Node: type %d, depth %d, score %d   (%llx)\n", entry->nodeType, entry->depth, entry->score, pvBoard.zobristHash);
                    pvMove = entry->bestMove;
                } else {
                    //printf("Node: type %d, depth %d, score %d   (%llx)\n", entry->nodeType, entry->depth, entry->score, pvBoard.zobristHash);
                    pvMove = entry->bestMove;
                    break;
                }
            }
            if (showBoard) {
                pvBoard.playMove(pvMove);
                printBoard(pvBoard);
            }
            if (uciInfos) {
                if (board.whiteTurn) {
                    printf("info score cp %d  depth %d\n", pvScore, currentDepth);
                } else {
                    printf("info score cp %d  depth %d\n", -pvScore, currentDepth);
                }
            }
        }

        endTime = std::chrono::system_clock::now();
        elapsedTime = (endTime-startTime).count() / 1000000.0f;

        if (verbose || uciInfos) {
            std::cout.flush();
        }

        currentDepth += 1;

        // If we will checkmate and the depth doesn't affect it, no need to search further
        /*if (lastBestResult.score == bestResult.score && (bestResult.score > CHECKMATE_BASE_SCORE - 100 || bestResult.score < -CHECKMATE_BASE_SCORE + 100)) {
            break;
        }*/

        #if MESURE_LEVEL >= ALL_MESURE
        if (verbose) {
            printf("| Best Move count for each index :\n");
            int indexesSum = 0;
            int indexesCount = 0;
            for (int i = 0 ; i < 10 ; i++) {
                indexesCount += bestMovesIndexes[i];
                indexesSum += bestMovesIndexes[i] * i;
                printf("|   %d -> %d   %f%\n", i, bestMovesIndexes[i], 100.0f*(float)bestMovesIndexes[i]/(float)movesIndexes[i]);
            }
            printf("| Average best move index : %f\n", (float)indexesSum/(float)indexesCount);
        }
        #endif
    }
    currentDepth -= 1;

    board.cleanPreviousHashes();    // Should not be called before the end of getBestMove

    stopSearchTimer();

    if (verbose) {
        #if MESURE_LEVEL >= LOW_MESURE
        printf("| Nodes : %d\n", nodeCount);
        #endif
        #if MESURE_LEVEL >= ALL_MESURE
        printf("| Evaluations : %d\n", evaluationCounter);
        printf("| Extensions : %d\n", extensionCount);
        printf("| PV hits : %d\n", PVHitCount);
        printf("| TT Hit : %d\n", TTHitCount);
        printf("| NMP : %d, Successes : %d\n", NMPCount, NMPPruneCount);
        printf("| LMR : %d, Fails : %d\n", LMRCount, LMRResearchCount);
        // printf("| TT Hit : %d, Collisions : %d\n", TTHitCount, TTCollisionCount);
        #endif
        printf("| Bot took %f milliseconds\n", elapsedTime);
        std::cout.flush();
    }

    if (!board.whiteTurn) {
        bestResult.score = -bestResult.score;
    }

    if (bestResult.move != NO_MOVE) {
        return bestResult;
    } else {
        return lastBestResult;
    }
}

void onMovePlayed(Board &board) {
    halfMoveTick += 1;

    if (principalVariation.size() > 0) {
        principalVariation.erase(principalVariation.begin());
    }
}

void onMoveUndone(Board &board) {
    halfMoveTick -= 1;
    principalVariation.clear();
}

// Does not reset TEST_VAR and searchId
void resetBot() {
    stopSearchTimer();
    halfMoveTick = 0;
    principalVariation.clear();

    /*for (int color = 0 ; color < 2 ; color++) {
        for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
            for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                moveHistory[color][startSquare][endSquare] = 0;
            }
        }
    }*/

    for (int i = 0 ; i < TTSize ; i++) {
        transpositionTable[i] = {};
    }
}

// For debugging - Prints some info about the status of the Transposition Table
void printTTInfos() {
    int entriesCount = 0;
    int allNodeCount = 0;
    int cutNodeCount = 0;
    int PVNodeCount = 0;
    int relativeDepthSum = 0;
    for (const TTEntry &entry : transpositionTable) {
        if (entry.nodeType != NO_NODE) {
            entriesCount += 1;
            relativeDepthSum += relativeTTDepth(entry);

            if (entry.nodeType == ALL_NODE) {
                allNodeCount += 1;
            } else if (entry.nodeType == CUT_NODE) {
                cutNodeCount += 1;
            } else if (entry.nodeType == PV_NODE) {
                PVNodeCount += 1;
            }
        }
    }

    float averageRelativeDepth = ((float) relativeDepthSum) / (float) entriesCount;

    printf("-- Transposition Table infos : --\n");
    printf("  Capacity : %d/%d\n", entriesCount, TTSize);
    printf("  Size : %d/%d Mo\n", (entriesCount * sizeof(TTEntry)) / 1000000, (TTSize * sizeof(TTEntry)) / 1000000);
    printf("  Average relative depth : %f\n", averageRelativeDepth);

    printf("  All nodes : %d\n", allNodeCount);
    printf("  Cut nodes : %d\n", cutNodeCount);
    printf("  PV nodes : %d\n", PVNodeCount);
}
void printMoveHistoryInfos() {
    /*printf("-- Move History infos : --\n");

    int * maximumValueBlackPtr = std::max_element(&moveHistory[BLACK][0][0], &moveHistory[BLACK][63][63]);
    int * maximumValueWhitePtr = std::max_element(&moveHistory[WHITE][0][0], &moveHistory[WHITE][63][63]);

    int maximumWhiteOffset = maximumValueWhitePtr - &moveHistory[WHITE][0][0];
    Move maximumMoveWhite = {maximumWhiteOffset / 64, maximumWhiteOffset % 64};

    int maximumBlackOffset = maximumValueBlackPtr - &moveHistory[BLACK][0][0];
    Move maximumMoveBlack = {maximumBlackOffset / 64, maximumBlackOffset % 64};

    printf("  Maximum value white : %d\n  ", *maximumValueWhitePtr);
    printMove(maximumMoveWhite);
    printf("  Maximum value black : %d\n  ", *maximumValueBlackPtr);
    printMove(maximumMoveBlack);

    // int * minimumValuePtr = std::min_element(&moveHistory[BLACK][0][0], &moveHistory[WHITE][63][63]);
    // printf("Minimum value : %d\n", *minimumValuePtr);*/
}

void stopSearch() {
    stopSearchTimer();
    isSearchCanceled = true;
}

private:

int initialPreviousHashesSize;    // Used to find the fastest checkmate, number of moves since last clock reset
char currentRepetitionCount = 0;
bool isVerbose;

// Incremented each time a timer is stopped or a new search is made
std::atomic<int> searchId = 0;
std::atomic<bool> isSearchCanceled = false;

// Ends the search and returns the current best move
void scheduleSearchTimer(std::chrono::milliseconds duration) {
    std::thread cancelationThread (&Bot::searchTimer, this, duration, searchId.load());
    cancelationThread.detach();
}

void stopSearchTimer() {
    searchId += 1;
}

void searchTimer(std::chrono::milliseconds &&duration, int &&currentSearchId) {
    std::this_thread::sleep_for(duration);

    if (searchId.load() == currentSearchId) {
        if (isVerbose) {
            printf("SEARCH CANCELLED\n");
            std::cout.flush();
        }
        isSearchCanceled = true;
    }
}

};
