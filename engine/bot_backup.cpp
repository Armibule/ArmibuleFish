#include "board.cpp"
#include "botConstants.cpp"
#include "fen.cpp"
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

enum NodeType : unsigned char {
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
    
    // int stability = 0; 
    short bestKeptCount = 0;
    short creationTick = 0;    // Tick of creation
    
    int rawStaticEval = 0;  // The static evaluation with correction history
};

// For debug purposes
void printTTEntry(const TTEntry &entry) {
    printf("TTEntry(depth %d", entry.depth);
    printf(", score %d", entry.score);
    printf(", nodeType %d", entry.nodeType);
    printf(", zobristHash %llx", entry.zobristHash);
    // printf(", creationTick %d)\n", entry.creationTick);
    printf(" -> Stored move: ");
    printMove(entry.bestMove);
}
void printSpaces(int depth) {
    for (int i = 0 ; i < NORMAL_DEPTH-depth ; i++) {
        printf("  ");
    }
}

bool moveResultCompareIncreasing(const MoveResult &a, const MoveResult &b) {
    return a.score < b.score;
}
bool moveResultCompareDecreasing(const MoveResult &a, const MoveResult &b) {
    return a.score > b.score;
}
bool moveResultCompareIncreasingPtr(const MoveResult * a, const MoveResult * b) {
    return (*a).score < (*b).score;
}
bool moveResultCompareDecreasingPtr(const MoveResult * a, const MoveResult * b) {
    return (*a).score > (*b).score;
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
float elapsedTime = 0.0f;

// Here the score is relative
MoveResult currentResult;

// Debug infos
#if MESURE_LEVEL >= LOW_MESURE
int nodeCount = 1;
int kNPS = 0;
#endif
#if MESURE_LEVEL >= ALL_MESURE
int evaluationCounter = 0;
int extensionCount = 0;
// int PVHitCount = 0;
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
int currentDepth = NORMAL_DEPTH;       // Depth of the current iterative deepening iteration


Bot() {
    resetBot();
}


// Indexed by color, pawnKey
int pawnCorrectionHistory[2][PAWN_CORRHIST_SIZE] = {};

inline uint64_t pawnCorrectionKey(const Board &board) {
    return ((board.colorBB[BLACK][PAWN] | board.colorBB[WHITE][PAWN]) * PAWN_CORRHIST_MAGIC) >> (64ULL - PAWN_CORRHIST_BITS);
}
void updatePawnCorrection(const Board &board, int depth, int evalScore, int searchScore) {
    int &correction = pawnCorrectionHistory[board.whiteTurn][pawnCorrectionKey(board)];

    // Adding quadratic term is mostly neutral (-3.47 +/- 25.01, 300 games)
    int t = std::min(depth, 20); // + (depth*depth)/32, 32); // 20);

    correction = std::clamp((correction*(512 - t) + (searchScore-evalScore)*t*MATERIAL_CORRECTION_FACTOR)/512, -MAX_PAWN_CORRECTION, MAX_PAWN_CORRECTION);
}
inline int applyPawnCorrection(const Board &board, int evalScore) {
    return evalScore + pawnCorrectionHistory[board.whiteTurn][pawnCorrectionKey(board)]/PAWN_CORRECTION_FACTOR;
}



// WIP : Material corrhist

// Indexed by color, materialKey
int materialCorrectionHistory[2][MATERIAL_CORRHIST_SIZE] = {};

inline uint64_t materialCorrectionKey(const Board &board) {
    return ((board.blackPieces[PAWN] * MATERIAL_CORRHIST_MAGICS[BLACK][PAWN]) ^
            (board.blackPieces[BISHOP] * MATERIAL_CORRHIST_MAGICS[BLACK][BISHOP]) ^
            (board.blackPieces[KNIGHT] * MATERIAL_CORRHIST_MAGICS[BLACK][KNIGHT]) ^
            (board.blackPieces[ROOK] * MATERIAL_CORRHIST_MAGICS[BLACK][ROOK]) ^
            (board.blackPieces[QUEEN] * MATERIAL_CORRHIST_MAGICS[BLACK][QUEEN]) ^
            (board.whitePieces[PAWN] * MATERIAL_CORRHIST_MAGICS[WHITE][PAWN]) ^
            (board.whitePieces[BISHOP] * MATERIAL_CORRHIST_MAGICS[WHITE][BISHOP]) ^
            (board.whitePieces[KNIGHT] * MATERIAL_CORRHIST_MAGICS[WHITE][KNIGHT]) ^
            (board.whitePieces[ROOK] * MATERIAL_CORRHIST_MAGICS[WHITE][ROOK]) ^
            (board.whitePieces[QUEEN] * MATERIAL_CORRHIST_MAGICS[WHITE][QUEEN])) >> (64ULL - MATERIAL_CORRHIST_BITS);
}
void updateMaterialCorrection(const Board &board, int depth, int evalScore, int searchScore) {
    int &correction = materialCorrectionHistory[board.whiteTurn][materialCorrectionKey(board)];

    // Adding quadratic term is mostly neutral (-3.47 +/- 25.01, 300 games)
    int t = std::min(depth, 20); // + (depth*depth)/32, 32);

    correction = std::clamp((correction*(512 - t) + (searchScore-evalScore)*t*MATERIAL_CORRECTION_FACTOR)/512, -MAX_MATERIAL_CORRECTION, MAX_MATERIAL_CORRECTION);
}
inline int applyMaterialCorrection(const Board &board, int evalScore) {
    return evalScore + materialCorrectionHistory[board.whiteTurn][materialCorrectionKey(board)]/MATERIAL_CORRECTION_FACTOR;
}


// Indexed by color, whiteKingSquare, blackKingSquare
/*int kingsCorrectionHistory[2][64][64] = {};


void updateKingsCorrection(const Board &board, int depth, int evalScore, int searchScore) {
    int &correction = kingsCorrectionHistory[board.whiteTurn][board.whiteKingSquare][board.blackKingSquare];

    int t = std::min(depth, 20);

    correction = std::clamp((correction*(512 - t) + (searchScore-evalScore)*t*KINGS_CORRECTION_FACTOR)/512, -MAX_KINGS_CORRECTION, MAX_KINGS_CORRECTION);
}
inline int applyKingsCorrection(const Board &board, int evalScore) {
    return evalScore + kingsCorrectionHistory[board.whiteTurn][board.whiteKingSquare][board.blackKingSquare]/KINGS_CORRECTION_FACTOR;
}*/


inline void updateCorrection(const Board &board, int depth, int evalScore, int searchScore) {
    evalScore = std::clamp(evalScore, -25'00, 25'00);
    searchScore = std::clamp(searchScore, -25'00, 25'00);
    updatePawnCorrection(board, depth, evalScore, searchScore);
    updateMaterialCorrection(board, depth, evalScore, searchScore);
    // updateKingsCorrection(board, depth, evalScore, searchScore);
}
inline int applyCorrection(const Board &board, int evalScore) {
    return /*applyKingsCorrection(board, */applyMaterialCorrection(board, applyPawnCorrection(board, evalScore))/*)*/;
}




inline int drawScore(const Board &board) {
    // Adds a tiny bit of randomness in drawn positions to maybe help ordering (neutral)
    return (board.zobristHash & 0b11u) - 2;
}
// Helps checkmates in endgames
/*inline int endgameMatingBonus(const Board &board) {
    if (std::popcount(board.allOccupancy) == 3) {
        bool whiteAdvantage = board.colorBB[WHITE][ROOK] || board.colorBB[WHITE][QUEEN];

        int bonus = 0;
        if (whiteAdvantage) {
            bonus = 500*(std::abs(4-squareX(board.blackKingSquare)) + std::abs(4-squareY(board.blackKingSquare))) - 200*(std::abs(squareX(board.whiteKingSquare)-squareX(board.blackKingSquare)) + std::abs(squareY(board.whiteKingSquare)-squareY(board.blackKingSquare)));
        } else {
            bonus = -500*(std::abs(4-squareX(board.whiteKingSquare)) + std::abs(4-squareY(board.whiteKingSquare))) + 200*(std::abs(squareX(board.whiteKingSquare)-squareX(board.blackKingSquare)) + std::abs(squareY(board.whiteKingSquare)-squareY(board.blackKingSquare)));
        }

        if (board.whiteTurn) {
            return bonus;
        } else {
            return -bonus;
        }
    }
    return 0;
}*/
// Returns the relative static score of the position
// Final deth is used to find the fastest checkmate
int evaluatePosition(Board &board) {
    switch (board.state) {
    case WHITE_WON:
        if (board.whiteTurn) {
            return CHECKMATE_BASE_SCORE + initialPly - board.ply;  // Go for the fastest checkmate + avoid loops
        } else {
            return -CHECKMATE_BASE_SCORE - initialPly + board.ply;
        }
    case BLACK_WON:
        if (board.whiteTurn) {
            return -CHECKMATE_BASE_SCORE - initialPly + board.ply;
        } else {
            return CHECKMATE_BASE_SCORE + initialPly - board.ply;
        }
    case DRAW:
        return drawScore(board);
    }

    if (board.repetitionCount > currentRepetitionCount) {
        return drawScore(board);
    }

    #if MESURE_LEVEL >= ALL_MESURE
    evaluationCounter += 1;
    #endif

    // board.applyNNUEAccumulatorsChanges();   // TESTEEE
    int outputBucketIndex = NNUE::outputBucketIndex(board.allOccupancy);

    int score;
    /*if (board.whiteTurn) {
        score = nnue->feedForward(board.whiteAccumulator, board.blackAccumulator, outputBucketIndex);
    } else {
        score = nnue->feedForward(board.blackAccumulator, board.whiteAccumulator, outputBucketIndex);
    }*/
    if (board.whiteTurn) {
        score = nnue->feedForward(board.whiteAccumulatorStack.back(), board.blackAccumulatorStack.back(), outputBucketIndex);
    } else {
        score = nnue->feedForward(board.blackAccumulatorStack.back(), board.whiteAccumulatorStack.back(), outputBucketIndex);
    }

    // TODO : Test if it improves ? -> looks ok
    if (board.isInCheck(board.whiteTurn)) {
        score -= checkValue;
    }

    // score += endgameMatingBonus(board); // TESTAAA

    score = applyCorrection(board, score);

    // Anticipates better drawn endgames ? BAD
    /*if (std::popcount(board.occupencies[WHITE]) == 2 && (board.colorBB[WHITE][BISHOP] | board.colorBB[WHITE][KNIGHT])) {
        if (board.whiteTurn) {
            score = std::min(score, 0);
        } else {
            score = std::max(score, 0);
        }
    }
    if (std::popcount(board.occupencies[BLACK]) == 2 && (board.colorBB[BLACK][BISHOP] | board.colorBB[BLACK][KNIGHT])) {
        if (board.whiteTurn) {
            score = std::max(score, 0);
        } else {
            score = std::min(score, 0);
        }
    }*/

    // --- BOT PLAYSTYLE VARIANT TEST ---
    // To make the black bot love to eat pawns
    /*int pawnBonus = 500*(std::popcount(board.colorBB[WHITE][PAWN]) - std::popcount(board.colorBB[BLACK][PAWN]));
    if (board.whiteTurn) {
        score += pawnBonus;
    } else {
        score -= pawnBonus;
    }*/
    // To remove material values to only keep positionnal values
    /*int absoluteMaterial = 
        piecesStandardValue[PAWN]     * board.whitePieces[PAWN]
        + piecesStandardValue[BISHOP] * board.whitePieces[BISHOP]
        + piecesStandardValue[KNIGHT] * board.whitePieces[KNIGHT]
        + piecesStandardValue[ROOK]   * board.whitePieces[ROOK]
        + piecesStandardValue[QUEEN]  * board.whitePieces[QUEEN]
        - piecesStandardValue[PAWN]   * board.blackPieces[PAWN]
        - piecesStandardValue[BISHOP] * board.blackPieces[BISHOP]
        - piecesStandardValue[KNIGHT] * board.blackPieces[KNIGHT]
        - piecesStandardValue[ROOK]   * board.blackPieces[ROOK]
        - piecesStandardValue[QUEEN]  * board.blackPieces[QUEEN];
    int relativeMaterial;
    if (board.whiteTurn) {
        relativeMaterial = absoluteMaterial;
    } else {
        relativeMaterial = -absoluteMaterial;
    }
    score = score - relativeMaterial;*/

    // Maybe helps in draws ?
    /*if (score > 0) {
        score = std::max(score + (initialPly - board.ply)/2, 0);
    } else {
        score = std::min(score - (initialPly - board.ply)/2, 0);
    }*/

    return score;
}
// Retrieve real eval using evaluatePositionFromRaw
// Intended to be stored in Transposition table for later use
int evaluatePositionRaw(Board &board) {
    switch (board.state) {
    case WHITE_WON:
        if (board.whiteTurn) {
            return CHECKMATE_BASE_SCORE + initialPly - board.ply;  // Go for the fastest checkmate + avoid loops
        } else {
            return -CHECKMATE_BASE_SCORE - initialPly + board.ply;
        }
    case BLACK_WON:
        if (board.whiteTurn) {
            return -CHECKMATE_BASE_SCORE - initialPly + board.ply;
        } else {
            return CHECKMATE_BASE_SCORE + initialPly - board.ply;
        }
    case DRAW:
        return drawScore(board);
    }

    if (board.repetitionCount > currentRepetitionCount) {
        return drawScore(board);
    }

    #if MESURE_LEVEL >= ALL_MESURE
    evaluationCounter += 1;
    #endif

    // board.applyNNUEAccumulatorsChanges();   // TESTEEE
    int outputBucketIndex = NNUE::outputBucketIndex(board.allOccupancy);

    int rawScore;
    /*if (board.whiteTurn) {
        score = nnue->feedForward(board.whiteAccumulator, board.blackAccumulator, outputBucketIndex);
    } else {
        score = nnue->feedForward(board.blackAccumulator, board.whiteAccumulator, outputBucketIndex);
    }*/
    if (board.whiteTurn) {
        rawScore = nnue->feedForward(board.whiteAccumulatorStack.back(), board.blackAccumulatorStack.back(), outputBucketIndex);
    } else {
        rawScore = nnue->feedForward(board.blackAccumulatorStack.back(), board.whiteAccumulatorStack.back(), outputBucketIndex);
    }

    // TODO : Test if it improves ? -> looks ok
    if (board.isInCheck(board.whiteTurn)) {
        rawScore -= checkValue;
    }

    // rawScore += endgameMatingBonus(board); // TESTAAA

    // --- BOT PLAYSTYLE VARIANT TEST ---
    // To make the black bot love to eat pawns
    /*int pawnBonus = 500*(std::popcount(board.colorBB[WHITE][PAWN]) - std::popcount(board.colorBB[BLACK][PAWN]));
    if (board.whiteTurn) {
        score += pawnBonus;
    } else {
        score -= pawnBonus;
    }*/
    // To remove material values to only keep positionnal values
    /*int absoluteMaterial = 
        piecesStandardValue[PAWN]     * board.whitePieces[PAWN]
        + piecesStandardValue[BISHOP] * board.whitePieces[BISHOP]
        + piecesStandardValue[KNIGHT] * board.whitePieces[KNIGHT]
        + piecesStandardValue[ROOK]   * board.whitePieces[ROOK]
        + piecesStandardValue[QUEEN]  * board.whitePieces[QUEEN]
        - piecesStandardValue[PAWN]   * board.blackPieces[PAWN]
        - piecesStandardValue[BISHOP] * board.blackPieces[BISHOP]
        - piecesStandardValue[KNIGHT] * board.blackPieces[KNIGHT]
        - piecesStandardValue[ROOK]   * board.blackPieces[ROOK]
        - piecesStandardValue[QUEEN]  * board.blackPieces[QUEEN];
    int relativeMaterial;
    if (board.whiteTurn) {
        relativeMaterial = absoluteMaterial;
    } else {
        relativeMaterial = -absoluteMaterial;
    }
    rawScore = rawScore - relativeMaterial;*/

    return rawScore;
}
// Faster than recomputing the whole evaluation
int evaluatePositionFromRaw(Board &board, int rawScore) {
    switch (board.state) {
    case WHITE_WON:
        if (board.whiteTurn) {
            return CHECKMATE_BASE_SCORE + initialPly - board.ply;  // Go for the fastest checkmate + avoid loops
        } else {
            return -CHECKMATE_BASE_SCORE - initialPly + board.ply;
        }
    case BLACK_WON:
        if (board.whiteTurn) {
            return -CHECKMATE_BASE_SCORE - initialPly + board.ply;
        } else {
            return CHECKMATE_BASE_SCORE + initialPly - board.ply;
        }
    case DRAW:
        return drawScore(board);
    }

    if (board.repetitionCount > currentRepetitionCount) {
        return drawScore(board);
    }

    // Anticipates better drawn endgames ? BAD
    /*if (std::popcount(board.occupencies[WHITE]) == 2 && (board.colorBB[WHITE][BISHOP] | board.colorBB[WHITE][KNIGHT])) {
        if (board.whiteTurn) {
            score = std::min(score, 0);
        } else {
            score = std::max(score, 0);
        }
    }
    if (std::popcount(board.occupencies[BLACK]) == 2 && (board.colorBB[BLACK][BISHOP] | board.colorBB[BLACK][KNIGHT])) {
        if (board.whiteTurn) {
            score = std::max(score, 0);
        } else {
            score = std::min(score, 0);
        }
    }*/

    // If the game is not decided, corrects the score
    int score = applyCorrection(board, rawScore);

    // Maybe helps in draws ?
    /*if (score > 0) {
        score = std::max(score + (initialPly - board.ply)/2, 0);
    } else {
        score = std::min(score - (initialPly - board.ply)/2, 0);
    }*/

    return score;
}

// Transposition table
int halfMoveTick = 0;
TTEntry transpositionTable[TTSize];

inline int getTTIndex(uint64_t zobristHash) {
    return zobristHash & TTMask;
}
inline int getTTIndex(const Board &board) {
    return getTTIndex(board.zobristHash);
}
// Will eventually be removed
/*inline int relativeTTDepth(const TTEntry &entry) {
    // test : keep only depth : 13.90 +/- 28.74 over 300 games
    return entry.depth; // - (halfMoveTick - entry.creationTick);
}*/
inline void updateTT(TTEntry &currentEntry, uint64_t zobristHash, int depth, const Move &bestMove, int score, NodeType nodeType, int rawStaticEval, int rootDistance) {
    if (currentEntry.nodeType == NO_NODE || 
        currentEntry.depth <= depth) {
        if (currentEntry.bestMove == bestMove) {
            currentEntry.bestKeptCount += 1;
        } else {
            currentEntry.bestKeptCount = 0;
        }
        // Lerps the stability score
        // if (currentEntry.zobristHash == zobristHash) {
        //     if (currentEntry.bestMove == bestMove) {
        //         currentEntry.stability = std::min((currentEntry.stability + MAX_TT_STABILITY)/2, MAX_TT_STABILITY);
        //     } else {
        //         // currentEntry.stability = std::max((currentEntry.stability - MAX_TT_STABILITY*3)/4, -MAX_TT_STABILITY);
        //         currentEntry.stability = std::max((currentEntry.stability - MAX_TT_STABILITY)/2, -MAX_TT_STABILITY);
        //     }
        // } else {
        //     currentEntry.stability = 0;
        // }

        // TESTAAA
        if (score > CHECKMATE_THRESHOLD) {
            score += rootDistance;
        } else if (score < -CHECKMATE_THRESHOLD) {
            score -= rootDistance;
        }
        
        currentEntry.zobristHash = zobristHash;
        currentEntry.depth = depth;
        currentEntry.bestMove = bestMove;
        currentEntry.score = score;
        currentEntry.nodeType = nodeType;
        currentEntry.creationTick = halfMoveTick;
        currentEntry.rawStaticEval = rawStaticEval;
    } else if (   // Not sure if this is good...
        currentEntry.zobristHash != zobristHash && 
        currentEntry.depth - (halfMoveTick - currentEntry.creationTick) + 2 < depth
    ) {
        // TESTAAA
        if (score > CHECKMATE_THRESHOLD) {
            score += rootDistance;
        } else if (score < -CHECKMATE_THRESHOLD) {
            score -= rootDistance;
        }

        currentEntry.zobristHash = zobristHash;
        currentEntry.depth = depth;
        currentEntry.bestMove = bestMove;
        currentEntry.score = score;
        currentEntry.nodeType = nodeType;
        currentEntry.bestKeptCount = 0;
        currentEntry.creationTick = halfMoveTick;
        currentEntry.rawStaticEval = rawStaticEval;
    }
}
inline void updateTT(const Board &board, int depth, const Move &bestMove, int score, NodeType nodeType, int rawStaticEval) {
    updateTT(transpositionTable[getTTIndex(board)], board.zobristHash, depth, bestMove, score, nodeType, rawStaticEval, board.ply - initialPly);
}
// Use with PV nodes
// Don't forget to use relative scores !
inline void updateTT_PV(const Board &board, int depth, const Move &bestMove, int score) {
    TTEntry &currentEntry = transpositionTable[getTTIndex(board)];
    currentEntry.zobristHash = board.zobristHash;
    currentEntry.depth = depth;
    currentEntry.bestMove = bestMove;
    // currentEntry.score = score; // BREAKS MATE SCORES // TESTAAA 
    currentEntry.nodeType = PV_NODE;
    currentEntry.bestKeptCount = 0;  // Removing loses strength
    currentEntry.creationTick = (short) halfMoveTick;
}
/*inline void updateTT_PV(const Board &board) {
    transpositionTable[getTTIndex(board)].nodeType = PV_NODE;
}*/
inline void clearTT() {
    for (int i = 0 ; i < TTSize ; i++) {
        transpositionTable[i] = {};
    }
}


// Move History Heuristic (TODO : to fine tune)
// Piece Type does not currently work
// Indexed by moveHistory[isWhite][startSquare][endSquare]
int moveHistory[2][64][64] = {};

inline void decayMoveHistory() {
    // Don't decay = good ?
    /*for (int color = 0 ; color < 2 ; color++) {
        for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
            for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                moveHistory[color][startSquare][endSquare] = (moveHistory[color][startSquare][endSquare]*moveHistoryDecayFactor) / 256;
            }
        }
    }*/
}
// multipliers are  31.35 +/- 42.18, 100 games
// depth 1 history rework is  22.62 +/- 35.55, 200 games
inline void addHistoryBonus(bool whiteTurn, const Move &move, int depth, bool multiplier=1) {
    /*moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::min(
        moveHistory[whiteTurn][move.startSquare][move.endSquare] + (depth*depth), 
        moveHistoryMaxValue
    );*/

    //  Lerps value (better)
    int t = std::min(depth /*+ (depth*depth)/16, 38)*/, 20) * multiplier;    // Bonuses can be amplified
    int &history = moveHistory[whiteTurn][move.startSquare][move.endSquare];
    history = std::min(
        (history*(/*256*/512 - t) + moveHistoryMaxValue*t) / 512/*256*/, 
        moveHistoryMaxValue
    );
}
inline void addHistoryMalus(bool whiteTurn, const Move &move, int depth, int multiplier=1) {
    /*moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::max(
        moveHistory[whiteTurn][move.startSquare][move.endSquare] - (depth*depth), 
        -moveHistoryMaxValue
    );*/

    //  Lerps value (better)
    int t = std::min(depth /*+ (depth*depth)/16, 30)*/, 20) * multiplier;
    int &history = moveHistory[whiteTurn][move.startSquare][move.endSquare];
    history = std::max(
        (history*(/*256*/512 - t) - moveHistoryMaxValue*t) / 512/*256*/,  // TESTXY
        -moveHistoryMaxValue
    );
}


// Indexed by captureMoveHistory[isWhite][capturingType][endSquare][capturedType]
/*int captureMoveHistory[2][6][64][5] = {};

inline void addCaptureHistoryBonus(bool whiteTurn, const Move &move, int depth, PieceType capturingType, PieceType capturedType) {
    if (capturedType == EMPTY) { capturedType = PAWN; }
    
    //  Lerps value
    int t = std::min(depth, 20);
    int &history = captureMoveHistory[whiteTurn][capturingType][move.endSquare][capturedType];
    history = std::min(
        (history*(256 - t) + captureMoveHistoryMaxValue*t) / 256, 
        captureMoveHistoryMaxValue
    );
}
inline void addCaptureHistoryMalus(bool whiteTurn, const Move &move, int depth, PieceType capturingType, PieceType capturedType) {
    if (capturedType == EMPTY) { capturedType = PAWN; }
    
    //  Lerps value
    int t = std::min(depth, 20);
    int &history = captureMoveHistory[whiteTurn][capturingType][move.endSquare][capturedType];
    history = std::max(
        (history*(256 - t) - captureMoveHistoryMaxValue*t) / 256, 
        -captureMoveHistoryMaxValue
    );
}*/


// Killer Moves Heuristic
// Indexed by ply-initialPly, 2 killers for each
Move killerMoves[MAX_DEPTH][2] = {};

inline void clearKillerMoves() {
    for (int ply = 0 ; ply < MAX_DEPTH ; ply++) {
        killerMoves[ply][0] = NO_MOVE;
        killerMoves[ply][1] = NO_MOVE;
    }
}
inline void addKillerMove(const Board &board, const Move &move) {
    Move firstKiller = killerMoves[board.ply - initialPly][0];

    if (move != firstKiller) {
        killerMoves[board.ply - initialPly][0] = move;
        killerMoves[board.ply - initialPly][1] = firstKiller;
    }
}


// Counter Moves Heuristic, 17.39 +/- 28.83 elo, 200 games
// TESTFFF
Move counterMoves[2][64][64] = {};
// Only if the move is quiet
inline void addCounterMove(const Move &lastMove, const Move &move, bool whiteTurnBeforeMove) {
    counterMoves[whiteTurnBeforeMove][lastMove.startSquare][lastMove.endSquare] = move;
}


std::vector<Move> principalVariation = {};



// Work in progress
/*Square smallestAttacker(Board &board, uint64_t occupancy, Square square) {
    
}
int see(Board &board, Square square) {
    // uint64_t playingOccupancy;
    // uint64_t otherOccupancy;
    // if (board.whiteTurn) {
    //     playingOccupancy = board.occupencies[WHITE];
    //     otherOccupancy = board.occupencies[BLACK];
    // } else {
    //     playingOccupancy = board.occupencies[BLACK];
    //     otherOccupancy = board.occupencies[WHITE];
    // }
    uint64_t occupency = board.allOccupancy;

    Square attacker;
    while ((attacker = smallestAttacker()) != -1)
}*/


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
    MoveResult * moveBaseEvaluationsPtr[moveCount] = {};

    for (int i = 0 ; i < moveCount ; i++) {
        Move move = moves[i];

        int value = seeCapture(board, move);

        moveBaseEvaluations[i] = {move, value};
        moveBaseEvaluationsPtr[i] = &moveBaseEvaluations[i];
    }

    // The value used is not color dependant
    // std::sort(moveBaseEvaluations, moveBaseEvaluations + moveCount, moveResultCompareDecreasing);
    std::sort(moveBaseEvaluationsPtr, moveBaseEvaluationsPtr + moveCount, moveResultCompareDecreasingPtr);

    for (int i = 0 ; i < moveCount ; i++) {
        MoveResult baseMoveResult = *moveBaseEvaluationsPtr[i]; //moveBaseEvaluations[i];
        // if (baseMoveResult.score <= -300) { break; }     // (doesn't work) Don't search moves with SEE < -300
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
void makeSearchExtensions(const Board &board, bool isInCheck, int &depth, int &remainingSearchExtensions, int &remainingHorizonExtensions, int moveCount) {
    if (depth == 1 && remainingHorizonExtensions > 0) {
        if (moveCount == 1) {
            depth += 1;
            remainingHorizonExtensions -= 1;
            return;
        }
        // Extends if in check
        if (isInCheck) {
            depth += 1;
            remainingHorizonExtensions -= 1;
            return;
        }
    } else if (remainingSearchExtensions > 0) {
        if (moveCount == 1) {
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

// Seems an ok condition
// bool inNMP = false;


// To test - probably not completely correct
bool isAttacked(Board &board, Square square) {
    bool whiteTurn = board.whiteTurn;
    uint64_t * otherBB = board.colorBB[!whiteTurn];

    if (board.capturesMask(square, KNIGHT, whiteTurn) & otherBB[KNIGHT]) { return true; }
    uint64_t virtualQueenMask = board.capturesMask(square, QUEEN, whiteTurn);
    if (virtualQueenMask & otherBB[QUEEN]) { return true; }
    if (virtualQueenMask & otherBB[PAWN]) {
        if (board.capturesMask(square, PAWN, whiteTurn) & otherBB[PAWN]) { return true; }
    }
    if (virtualQueenMask & otherBB[KING]) {
        if (board.capturesMask(square, KING, whiteTurn) & otherBB[KING]) { return true; }
    }
    if (virtualQueenMask & otherBB[BISHOP]) {
        if (board.capturesMask(square, BISHOP, whiteTurn) & (otherBB[BISHOP] | otherBB[QUEEN])) { return true; }
    }
    if (virtualQueenMask & otherBB[ROOK]) {
        if (board.capturesMask(square, ROOK, whiteTurn) & (otherBB[ROOK] | otherBB[QUEEN])) { return true; }
    }

    return false;
}
// Not well optimized, no legality checks, only works on captures, not tested
int see(uint64_t occupancy, const uint64_t * playingBB, const uint64_t * otherBB, Square square, bool whiteTurn, PieceType victimType) {

    // We do not consider the king (unsafe)
    for (int capturingType = PAWN ; capturingType <= QUEEN/*KING*/ ; capturingType++) {
        uint64_t mask = Board::rawAttacksMask(square, (PieceType) capturingType, !whiteTurn, occupancy);

        uint64_t candidatesBB = playingBB[capturingType] & occupancy & mask;
        uint64_t candidateBB = popLastBit(candidatesBB);

        // Idea : try every possibility when there are multiple candidates ?
        if (candidateBB) {
            occupancy ^= candidateBB;

            return std::max(
                0, 
                piecesStandardValue[victimType] - see(occupancy, otherBB, playingBB, square, !whiteTurn, (PieceType) capturingType)
            ); 
        }
    }

    return 0;
}
int see(const Board &board, Square square) {
    PieceType victimType = board.getAt(square).type;
    if (victimType == EMPTY) {
        victimType = PAWN;  // En passant
    }
    return see(board.allOccupancy, board.colorBB[board.whiteTurn], board.colorBB[!board.whiteTurn], 
               square, board.whiteTurn, victimType);
}
int seeCapture(const Board &board, const Move &move) {
    bool whiteTurn = board.whiteTurn;
    Square endSquare = move.endSquare;
    Square startSquare = move.startSquare;

    PieceType capturingPiece = board.getAt(startSquare).type;
    PieceType capturedPiece = board.getAt(endSquare).type;
    if (capturedPiece == EMPTY) {
        capturedPiece = PAWN;  // En passant
    }

    return piecesStandardValue[capturedPiece] - 
           see(board.allOccupancy^bit(startSquare), board.colorBB[!whiteTurn], board.colorBB[whiteTurn], endSquare, !whiteTurn, capturingPiece);
}
// Don't do on castling and promotions
int seeQuiet(const Board &board, const Move &move) {
    bool whiteTurn = board.whiteTurn;
    Square endSquare = move.endSquare;
    Square startSquare = move.startSquare;

    PieceType movingPiece = board.getAt(startSquare).type;

    return -see(board.allOccupancy^bit(startSquare), board.colorBB[!whiteTurn], board.colorBB[whiteTurn], endSquare, !whiteTurn, movingPiece);
}
// Computes the static exchange evaluation on a square - by the currently playing player 
/*int seeSquareAfterMove(const Board &board, Square square) {
    bool whiteTurn = board.whiteTurn;

    PieceType pieceType = board.getAt(square).type;

    return see(board.allOccupancy, board.colorBB[whiteTurn], board.colorBB[!whiteTurn], square, whiteTurn, pieceType);
}*/

/*
int seeQuiet(const Board &board, const Move &move) {
    bool whiteTurn = board.whiteTurn;
    Square endSquare = move.endSquare;
    Square startSquare = move.startSquare;

    PieceType movingPiece = board.getAt(startSquare).type;

    return -see(board.allOccupancy^bit(square), board.colorBB[!whiteTurn], board.colorBB[whiteTurn], square, !whiteTurn, movingPiece);
}
*/


std::vector<int> nullMovesPlies = {-1};

int PVSearch(Board &board, int depth=NORMAL_DEPTH, int alpha=-INFINITE_SCORE, int beta=INFINITE_SCORE, int remainingSearchExtensions=MAX_SEARCH_EXTENSION, int remainingHorizonExtensions=MAX_HORIZON_EXTENSION, const Move &lastMove=NO_MOVE) {
    if (isSearchCanceled) {
        // Worst score possible for the pervious layer of minmax
        // So this move will not be played
        return INFINITE_SCORE;
    }

    #if MESURE_LEVEL >= LOW_MESURE
    nodeCount += 1;
    #endif

    // The handling doesn't seem right ?
    if (board.state != NEUTRAL || board.repetitionCount > currentRepetitionCount) {
        return evaluatePosition(board);
    }

    /*if (depth >= currentDepth-2) {
        printSpaces(depth);
        printf("%d  alpha %d, beta %d   start (%llx)\n", depth, alpha, beta, board.zobristHash); 
    }*/

    bool whiteTurn = board.whiteTurn;
    Move bestMove = NO_MOVE;
    bool isPV = false;

    int rootDistance = board.ply - initialPly;

    // TESTAAA
    // Mate distance pruning
    int maxTheoreticalScore = CHECKMATE_BASE_SCORE - rootDistance;
    if (maxTheoreticalScore < beta) {
        beta = maxTheoreticalScore;

        if (alpha >= maxTheoreticalScore) {
            return maxTheoreticalScore;
        }
    }
    int minTheoreticalScore = -CHECKMATE_BASE_SCORE + rootDistance;
    if (minTheoreticalScore > alpha) {
        alpha = minTheoreticalScore;

        if (beta <= minTheoreticalScore) {
            return minTheoreticalScore;
        }
    }


    // Check if the position is present in the transposition table
    Move refutationMove = NO_MOVE;
    bool isRefutationMoveCapture = false;
    // bool isRefutationMovePawnMove = false;
    bool isExpectedCutNode = false;
    int staticScoreRaw;
    int staticScore;
    int baseScore;
    bool hasTT = false;
    TTEntry &currentEntry = transpositionTable[getTTIndex(board)];
    short bestKeptCount = 0;
    // int ttStability = 0;
    if (currentEntry.nodeType != NO_NODE && currentEntry.zobristHash == board.zobristHash) {

        // TESTAAA
        // In the transposition table, mate scores are stored relative to the current node
        // So we make it relative to the root
        // This is 12.75 +/- 24.75, 300 games with maybeMate flag
        int entryScore = currentEntry.score;
        bool maybeMate = false;
        if (entryScore > CHECKMATE_THRESHOLD) {
            entryScore -= rootDistance;
            maybeMate = true;
        } else if (entryScore < -CHECKMATE_THRESHOLD) {
            entryScore += rootDistance;
            maybeMate = true;
        }

        // if (currentEntry.depth > depth) {    //does not currently work
        // if (currentEntry.depth > depth && !maybeMate) {    // ASPITEST
        if (currentEntry.depth >= depth && !maybeMate) {    // TESTAAA
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  IS STORED\n", depth, alpha, beta); 
            }*/

            #if MESURE_LEVEL >= ALL_MESURE
            TTHitCount += 1;
            #endif

            switch (currentEntry.nodeType) {
            // SEEMS TO BE not exact, REMOVING IN TESTING
            case PV_NODE:
                // Condition almost passes
                //if (currentEntry.depth > depth) {  // TESTBBB
                    // The score is exact
                    return entryScore;
                //}
                
            case CUT_NODE:
                // Score is lower bound (relatively)
                if (entryScore >= beta) {
                    // Fail high
                    return entryScore;
                }
                if (entryScore > alpha) {
                    alpha = entryScore-1;
                }
                break;
            case ALL_NODE:
                // TEST
                // Score is upper bound (relatively)
                // if (currentEntry.score < alpha) { // Almost passes
                if (entryScore <= alpha) {
                    // Fail low
                    return entryScore;
                }
                if (entryScore < beta) {
                    beta = entryScore+1;
                }
                break;
            }

            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("  -> alpha %d, beta %d\n", alpha, beta); 
            }*/
        }

        isPV = currentEntry.nodeType == PV_NODE;
        isExpectedCutNode = currentEntry.nodeType == CUT_NODE;
        
        refutationMove = currentEntry.bestMove;
        baseScore = entryScore;

        isRefutationMoveCapture = board.isCapture(refutationMove);
        // isRefutationMovePawnMove = board.getAt(refutationMove.startSquare).type == PAWN;
        bestKeptCount = currentEntry.bestKeptCount;
        // ttStability = currentEntry.stability;

        hasTT = true;

        staticScoreRaw = currentEntry.rawStaticEval;
        staticScore = evaluatePositionFromRaw(board, staticScoreRaw);
    } else {
        staticScoreRaw = evaluatePositionRaw(board);
        staticScore = evaluatePositionFromRaw(board, staticScoreRaw);

        baseScore = staticScore; // evaluatePosition(board);
    }
    int correctionAmount = staticScore - staticScoreRaw;
    /*if (correctionAmount > 50) {
        std::cout << correctionAmount << "\n";
    }*/

    std::vector<Move> moves;
    board.getAllMoves(moves);

    const int moveCount = moves.size();

    if (moveCount == 0) {
        // This is a leaf node, will only be reached after null moves
        return baseScore;
    }

    bool isInCheck = board.isInCheck(whiteTurn);

    // Maybe something here is BROKEN at STC ?, try to remove some things
    // Static score mix is 24.36 +/- 31.08, 200 games
    int RFP_scoreMix = (staticScore+baseScore)/2;
    int NMP_scoreMix = RFP_scoreMix;
    // TESTGGG
    int RFP_marginModifier = 0;
    // TESTHHH
    int NMP_marginModifier = 0;     // Not that good
    // RFP margin moifier Does not improve much, (13.90 +/- 51.26, 100 games) but it looks good, and I don't care
    if (isExpectedCutNode) { RFP_marginModifier -= 20; } // NMP_marginModifier -= 15; }
    if (bestKeptCount > 10) { RFP_marginModifier -= 10; }
    if (std::abs(correctionAmount) < 10) { RFP_marginModifier -= 10; }
    else if (std::abs(correctionAmount) > 50) { RFP_marginModifier += 15; }

    //3.47 +/- 26.42, 300 games (neutral)
    /*if (isExpectedCutNode) { RFP_marginModifier -= 40; } // NMP_marginModifier -= 15; }
    if (bestKeptCount > 10) { RFP_marginModifier -= 20; }*/

    // Reverse futility pruning
    if (!isInCheck && 
        // (baseScore > beta + REVERSE_FUTILITY_MARGIN*depth) && 
        
        // (RFP_scoreMix > beta + REVERSE_FUTILITY_MARGIN*depth) &&
        // !isRefutationMoveCapture // TESTDDD

        // Reverse futility pruning on capture hash moves is 27.85 +/- 33.23, 200 games
        (RFP_scoreMix > beta + (REVERSE_FUTILITY_MARGIN_CAPTURE + RFP_marginModifier)*depth || (RFP_scoreMix > beta + (REVERSE_FUTILITY_MARGIN + RFP_marginModifier)*depth && !isRefutationMoveCapture)) &&

        !isPV &&
        baseScore < CHECKMATE_THRESHOLD    // TESTAAA
        ) {
        // Fail high
        // return baseScore;
        //  8.69 +/- 31.45, 200 games
        return (staticScore+baseScore)/2;
    }

    // Negative extension TEST, not good
    /*if (depth > 1 &&
        !isInCheck && 
        (RFP_scoreMix > beta + depth*(REVERSE_FUTILITY_MARGIN_CAPTURE - 150) || (RFP_scoreMix > beta + depth*(REVERSE_FUTILITY_MARGIN - 70) && !isRefutationMoveCapture)) &&
        !isPV &&
        baseScore < CHECKMATE_THRESHOLD &&
        isExpectedCutNode) {
        depth -= 1;
    }*/

    // TODO : TEST SEARCH EXTENSIONS !
    makeSearchExtensions(board, isInCheck, depth, remainingSearchExtensions, remainingHorizonExtensions, moveCount);

    // Null Move Pruning
    if (depth < currentDepth-1 && 
        depth > NullMovePruningReduction && 
        // Avoids zugzwangs
        (std::popcount(board.occupencies[whiteTurn]) > 1 + std::popcount(board.colorBB[whiteTurn][PAWN])) && // std::popcount(board.allOccupancy) > 6 && // std::popcount(board.allOccupancy) > 9 && // std::popcount(board.allOccupancy) > 5 && // (std::popcount(board.allOccupancy) > board.whitePieces[PAWN] + board.blackPieces[PAWN] + 2) && //  TO TEST !!!! std::popcount(board.allOccupancy) > 9 &&
        !isPV &&
        // Try only if the position looks good enough
        // (baseScore + NMPRejectMargin >= beta) &&
        (NMP_scoreMix + NMPRejectMargin + NMP_marginModifier >= beta) && // Mixing is very good : 41.89 +/- 35.71, 150 games
        !isInCheck &&
        // TESTFFF
        /*!inNMP*/
        // Just checking last ply 24.36 +/- 33.64, 200 games
        nullMovesPlies.back() != board.ply) {

        // inNMP = true;

        nullMovesPlies.push_back(board.ply);

        #if MESURE_LEVEL >= ALL_MESURE
        NMPCount += 1;
        #endif
        
        board.playNullMove();
        int nullSearchScore = -PVSearch(board, depth - NullMovePruningReduction, -beta, -beta + 1, remainingSearchExtensions, remainingHorizonExtensions/*, NO_MOVE*/);
        board.undoNullMove();

        // inNMP = false;
        nullMovesPlies.pop_back();

        if (nullSearchScore >= beta) {
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  NMP %d, alpha %d, beta %d\n", depth, nullSearchScore, alpha, beta); 
            }*/

            #if MESURE_LEVEL >= ALL_MESURE
            NMPPruneCount += 1;
            #endif

            // Test correction in NMP, not conclusive
            /*if (baseScore < nullSearchScore) {
                updateCorrection(board, depth, baseScore, nullSearchScore);
            }*/

            // NOT GOOD
            // Fail high, Cut node
            // Beware of the Null Move, depth is reduced to account for the shallower search
            // updateTT(currentEntry, board.zobristHash, depth-NullMovePruningReduction, NO_MOVE, nullSearchScore, CUT_NODE, staticScoreRaw, rootDistance);  //TEST123 updateTT(board, depth, move, score, CUT_NODE); 

            return nullSearchScore;     // Fail high
        }
    }

    // Removing condition = crazy boost
    //if (true || depth >= 1) {

    // Move ordering
    MoveResult moveEvaluations[moveCount] = {};

    Move killerMove1 = killerMoves[rootDistance][0];
    Move killerMove2 = killerMoves[rootDistance][1];
    // TESTFFF
    Move counterMove;
    if (lastMove == NO_MOVE) {
        counterMove = NO_MOVE;
    } else {
        counterMove = counterMoves[whiteTurn][lastMove.startSquare][lastMove.endSquare];
    }

    for (int i = 0 ; i < moveCount ; i++) {
        int value;
        Move move = moves[i];

        if (move == refutationMove) {
            value = refutationMoveBonus;
        } else {
            bool isCapture = board.isCapture(move);

            if (isCapture) {
                /*if (capturedPiece == EMPTY) {
                    // En passant
                    capturedPiece = PAWN;
                }*/
                // MVV-LVA ?
                /*value = captureBonus + piecesStandardValue[capturedPiece];
                
                // Is attacked - to test more ?
                if (isAttacked(board, move.endSquare)) {
                    value -= piecesStandardValue[capturingPiece];
                } else {
                    value -= piecesStandardValue[capturingPiece]/2;
                    // value -= piecesStandardValue[capturingPiece]/4; Does not work
                }*/
                
                // Better than MVV-LVA : 46.60 +/- 44.15 over 120 games !
                /*value = piecesStandardValue[capturedPiece];

                UnmakeMoveInfo info = board.playMove(move, false);
                value -= see(board, move.endSquare);
                board.undoMove(move, info, false);*/

                value = seeCapture(board, move);

                if (value > 0) {
                    value += positiveCaptureBonus;
                } /*else if (value == 0) {
                    value -= neutralCaptureMalus;
                }*//*else {
                    value /= 4;   // TEST
                }*/

                // PieceType capturedPiece = board.pieces[move.endSquare].type;
                // PieceType capturingPiece = board.pieces[move.startSquare].type;
                // value += captureMoveHistory[whiteTurn][capturingPiece][move.endSquare][capturedPiece]/captureMoveHistoryValueFactor;
            } else {
                if (move == killerMove1) {
                    value = killerMove1Bonus;
                } else if (move == killerMove2) {
                    value = killerMove2Bonus;
                } else {
                    value = moveHistory[whiteTurn][move.startSquare][move.endSquare]/moveHistoryValueFactor;
                }
                // TESTFFF
                if (move == counterMove) {
                    value += counterMoveBonus;
                }

                /*
                Very bad
                if (move.moveType == NORMAL_MOVE && move.promotionType == EMPTY) {
                    // Give penalties for moves that can loose material
                    if (seeQuiet(board, move) < 0) {
                        value -= 20;
                    }
                    // value // += seeQuiet(board, move) / 16;
                }*/
                /*if (move.moveType == NORMAL_MOVE && move.promotionType == EMPTY) {
                    // Give penalties for moves that can loose material
                    int seeVal = seeQuiet(board, move);
                    if (seeVal < -200) {
                        value -= 15;
                    } else if (seeVal < -100) {
                        value -= 8;
                    }
                }*/
                /*if (board.zobristHash % 177 == 0) {
                    printf("%d\n", value);
                }*/
            }

            // TEST (VERY BAD)
            /*UnmakeMoveInfo info = board.playMove(move, false);
            if (board.isInCheck(!whiteTurn)) {
                value += 50;
            }
            board.undoMove(move, info, false);
            UnmakeMoveInfo info = board.playMove(move, false);
            TTEntry entry = transpositionTable[getTTIndex(board)];
            if (entry.nodeType == CUT_NODE && currentEntry.zobristHash == board.zobristHash) {
                value += 50;
            }
            board.undoMove(move, info, false);*/
            
            if (move.promotionType == QUEEN) {
                value += promotionBonusQueen;
            }
        }

        moveEvaluations[i] = {move, value};
    }

    std::sort(moveEvaluations, moveEvaluations + moveCount, moveResultCompareDecreasing);

    for (int i = 0 ; i < moveCount ; i++) {
        moves[i] = moveEvaluations[i].move;
    }
    //}

    int LMRLevel = 0;       // Is increased during search
    int score;

    // Deep futility pruning TEST : SUPER BAD
    /*if (!isInCheck && 
        (baseScore + 400*depth < alpha) && 
        !isPV) {
        // Fail low
        return alpha;
    }*/

    if (depth == 1) {
        // Frontier node
        
        // TESTXY
        // for (const Move &move : moves) {
        for (int moveIndex = 0 ; moveIndex < moveCount ; moveIndex++) {
            Move move = moves[moveIndex];
            bool isCapture = board.isCapture(move);

            // Could benefit from lazy evaluation -> less frquent Accumulator updates
            UnmakeMoveInfo info = board.playMove(move); // , false);
            
            // Futility pruning, to improve BAD, removing is  24.36 +/- 33.99
            /*if (!isInCheck && 
                (baseScore + FUTILITY_MARGIN < alpha) && 
                !isCapture &&
                !isPV &&
                !board.isInCheck(!whiteTurn)) {
                board.undoMove(move, info, false);
                continue;
            } // TESTAAA*/

            // Updates since it wasn't done by playMove()
            // board.updateNNUEAccumulators(info.accumulatorChanges);

            score = -quiescenceSearch(board, -beta, -alpha);
            board.undoMove(move, info);

            if (score >= beta) {
                // Only updates on quiet moves
                if (!isCapture) {
                    addHistoryBonus(whiteTurn, move, depth, 1);
                    addKillerMove(board, move);
                    // addCounterMove(lastMove, move);

                    // TESTXY
                    for (int j = 0 ; j < moveIndex ; j++) {
                        Move quietMove = moves[j];
                        if (!board.isCapture(quietMove)) {
                            addHistoryMalus(whiteTurn, quietMove, depth, 1);
                        }
                    }

                } /*else {
                    addCaptureHistoryBonus(whiteTurn, move, depth, board.getAt(bestMove.startSquare).type, board.getAt(move.endSquare).type);
                }*/


                // Depth 1 corrhist test
                // Currently neutral, slightly bad : does not pass
                /*if (!hasTT &&
                    !isInCheck &&
                    !board.isCapture(move) &&
                    baseScore < score) {
                    updateCorrection(board, depth, baseScore, score);
                }*/

                // Fail high, Cut node
                updateTT(currentEntry, board.zobristHash, depth, move, score, CUT_NODE, staticScoreRaw, rootDistance); //TEST123 updateTT(board, depth, move, score, CUT_NODE);
                return score;
            }
            if (score > alpha) {
                bestMove = move;
                alpha = score;
            } /*else {  // TESTXY
                if (!isCapture) {
                    addHistoryMalus(whiteTurn, move, depth);
                }
            }*/
        }

        // TESTXY
        if (!board.isCapture(bestMove)) {
            addHistoryBonus(whiteTurn, bestMove, depth, 1);

            for (int j = 0 ; j < moveCount ; j++) {
                Move quietMove = moves[j];

                if (quietMove == bestMove) {
                    continue;
                }

                if (!board.isCapture(quietMove)) {
                    addHistoryMalus(whiteTurn, quietMove, depth, 1);
                }
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
        // Fallback move in case none increase alpha (useful in iterative deepening)
        bestMove = firstMove;

        
        UnmakeMoveInfo info = board.playMove(firstMove);

        /*if (isRefutationMoveCapture && isPV) {
        score = -PVSearch(board, depth, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions, firstMove);
        } else {*/
            // !!! BASIS !!!
            score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions, firstMove);
        // }

        // ASPITEST (Trash)
        /*int windowAlpha = std::max(alpha, (alpha+beta)/2 - 50);
        int windowBeta = std::min(beta, (alpha+beta)/2 + 50);
        // Aspiration window test
        score = -PVSearch(board, depth - 1, -windowBeta, -windowAlpha, remainingSearchExtensions, remainingHorizonExtensions, firstMove);
        if (score < windowAlpha || score > windowBeta) {
            int score1 = score;
            // Research if outside window
            score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions, firstMove);
            // std::cout << windowAlpha << " , " << windowBeta << " : " << score1 << " -> " << score << "\n";
        }*/

        // if (score == -INFINITE_SCORE) { return INFINITE_SCORE; }

        // ASPIRATION WINDOW TEST (very bad)
        /*
        // if (beta - alpha > 1 && alpha <= baseScore && baseScore <= beta) {
        //     int alphaBound = std::max(alpha, baseScore-50);
        if (alpha+200 < beta) {
            int alphaBound = alpha + 100;
            // int betaBound = std::min(beta, baseScore+50);

            // score = -PVSearch(board, depth - 1, -betaBound, -alphaBound, remainingSearchExtensions, remainingHorizonExtensions);        
            // + Reduction ????? BAD TEST
            score = -PVSearch(board, depth - 1, -beta, -alphaBound, remainingSearchExtensions, remainingHorizonExtensions);        

            if (score < beta && score < alphaBound) {
                // Research !
                score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions);        
            }
        } else {
            score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions);        
        }
        */
        
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

            if (!board.isCapture(bestMove)) {
                addHistoryBonus(whiteTurn, bestMove, depth, 6*2);
                addKillerMove(board, bestMove);
                addCounterMove(lastMove, bestMove, whiteTurn);
            } /*else {
                addCaptureHistoryBonus(whiteTurn, bestMove, depth, board.getAt(bestMove.startSquare).type, board.getAt(bestMove.endSquare).type);
            }*/

            // Current conditions can be improved !
            if (//!hasTT &&
                !isInCheck &&
                !board.isCapture(firstMove) &&
                baseScore < score) {

                updateCorrection(board, depth, baseScore, score);
            }

            updateTT(currentEntry, board.zobristHash, depth, firstMove, score, CUT_NODE, staticScoreRaw, rootDistance); //TEST123 updateTT(board, depth, firstMove, score, CUT_NODE);     // Fail high, Cut node
            return score;
        }
        if (score > alpha) {
            alpha = score;

            /*if (depth >= currentDepth-2) {
            printSpaces(depth);
            printf("%d  alpha %d, beta %d  FIRST\n", depth, alpha, beta); 
            }*/
        } else {
            // TEST mid-bad
            /*if (!board.isCapture(firstMove)) {
                addHistoryMalus(whiteTurn, firstMove, depth);
            }*/
        }

        const int rootDistance = currentDepth-depth;

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

            // Test depth condition
            if (nodeDepth > LMRLevel + 1) {
                if (depth == currentDepth) {
                    //  Good : 52.51 +/- 48.41 elo, 120 games
                    switch (LMRLevel) {
                    case 0:
                        if (moveIndex >= 2) { LMRLevel = 1; }
                        break;
                    case 1:
                        if (moveIndex >= 7) { LMRLevel = 2; }
                        break;
                    case 2:
                        if (moveIndex >= 12) { LMRLevel = 3; }
                        break;
                    case 3:
                        if (moveIndex >= 20) { LMRLevel = 4; }
                        break;
                    }
                } else if (rootDistance < 3) {
                    switch (LMRLevel) {
                    case 0:
                        if (moveIndex >= LMR1_LOWDEPTH_MOVE_NUMBER) { LMRLevel = 1; }
                        break;
                    case 1:
                        if (moveIndex >= LMR2_LOWDEPTH_MOVE_NUMBER) { LMRLevel = 2; }
                        break;
                    case 2:
                        if (moveIndex >= LMR3_LOWDEPTH_MOVE_NUMBER) { LMRLevel = 3; }
                        break;
                    case 3:
                        if (moveIndex >= LMR4_LOWDEPTH_MOVE_NUMBER) { LMRLevel = 4; }
                        break;
                    }
                } else {
                    switch (LMRLevel) {
                    case 0:
                        if (moveIndex >= LMR1_MOVE_NUMBER) { LMRLevel = 1; }
                        break;
                    case 1:
                        if (moveIndex >= LMR2_MOVE_NUMBER) { LMRLevel = 2; }
                        break;
                    case 2:
                        if (moveIndex >= LMR3_MOVE_NUMBER) { LMRLevel = 3; }
                        break;
                    case 3:
                        if (moveIndex >= LMR4_MOVE_NUMBER) { LMRLevel = 4; }
                        break;
                    }
                }
            }
            /*if (nodeDepth > LMRLevel + 1) {
                switch (LMRLevel) {
                case 0:
                    if (moveIndex >= LMR1_MOVE_NUMBER) { LMRLevel = 1; }
                    break;
                case 1:
                    if (moveIndex >= LMR2_MOVE_NUMBER) { LMRLevel = 2; }
                    break;
                case 2:
                    if (moveIndex >= LMR3_MOVE_NUMBER) { LMRLevel = 3; }
                    break;
                case 3:
                    if (moveIndex >= LMR4_MOVE_NUMBER) { LMRLevel = 4; }
                    break;
                }
            }*/

            Move move = moves[moveIndex];
            //bool isCapture = board.isCapture(move);
            UnmakeMoveInfo info = board.playMove(move);

            // Futility pruning, not proven to work
            /*if (!isInCheck && 
                (baseScore + FUTILITY_MARGIN*depth < alpha) && 
                !isCapture &&
                !isPV &&
                !board.isInCheck(!whiteTurn)) {
                board.undoMove(move, info);
                continue;
            }*/

            if (board.state != NEUTRAL || board.repetitionCount > currentRepetitionCount) {
                score = -evaluatePosition(board);
            } else {
                // TEST
                /*int adjustedLMR = LMRLevel;
                
                if (isRefutationMoveCapture && (adjustedLMR < 3)) {
                    adjustedLMR += 1;
                }
                //if (isExpectedCutNode && (LMRLevel < 2)) {
                //    adjustedLMR += 1;
                //}
                if (isInCheck || board.isInCheck(board.whiteTurn)) {
                    // Reduce less when in PV node / when giving check / when in check
                    adjustedLMR -= 1;
                }
                if (isPV) {
                    adjustedLMR -= 1;
                }
                if (move == killerMove1 || move == killerMove2) {
                    adjustedLMR -= 1;
                }*/
                

                /*if (isInCheck || isPV || board.isInCheck(board.whiteTurn)) {
                    // Reduce less when in PV node / when giving check / when in check
                    adjustedLMR -= 1;
                }
                if (isExpectedCutNode && (adjustedLMR < 2)) {
                    // Reduce more when in expected cut node
                    adjustedLMR += 1;
                }
                if (isRefutationMoveCapture && (adjustedLMR < 3)) {
                    // Reduce more when the hash move is a capture
                    adjustedLMR += 1;
                }
                */
                /*adjustedLMR = std::clamp(adjustedLMR, 0, nodeDepth-1);
                nodeDepth -= adjustedLMR;
                #if MESURE_LEVEL >= ALL_MESURE
                if (LMRLevel > 0) {
                    LMRCount += 1;
                }
                #endif*/


                /* TEST : moyen, ne passe pas en l'état
                int orderingScore = moveEvaluations[moveIndex].score;
                if (orderingScore > 3000) {
                    LMRLevel = 0;
                } else if (orderingScore > 1000) {
                    LMRLevel = 1;
                } else if (orderingScore > 0) {
                    LMRLevel = 2;
                } else if (orderingScore > -300) {
                    LMRLevel = 3;
                } else {
                    LMRLevel = 4;
                }
                LMRLevel = std::min(LMRLevel, nodeDepth-1);*/


                //if (isInCheck || isPV   /*|| (move == killerMove1 || move == killerMove2) does not work*//*|| board.isInCheck(board.whiteTurn)*/) {
                //    // Reduce less when in PV node / when giving check / when in check
                //    nodeDepth -= std::max(LMRLevel - 1, 0);
                //} else if (isRefutationMoveCapture && (LMRLevel < 3)) {
                //    // Reduce more when the hash move is a capture
                //    // TODO : Is this condition better ? Seems ok !
                //    nodeDepth -= std::min(LMRLevel + 1, nodeDepth - 1);
                //} /*else if (isExpectedCutNode && (LMRLevel < 2)) {
                //    // Reduce more when in expected cut node
                //    // TODO : Is this condition better ? Not for now
                //    nodeDepth -= std::min(LMRLevel + 1, nodeDepth - 1);
                //}*/ else {
                //    /* Does not work
                //    if ((LMRLevel > 2) && (board.getAt(move.startSquare).type == PAWN)) {
                //        LMRLevel -= 1;  // Reduce pawn moves less ?
                //    }*/
                //    /*if (nodeDepth > 1 && bestKeptCount > 5 && (LMRLevel < 3)) {
                //       nodeDepth -= 1;
                //    }*/
                //    nodeDepth -= LMRLevel;
                //    #if MESURE_LEVEL >= ALL_MESURE
                //    if (LMRLevel > 0) {
                //        LMRCount += 1;
                //    }
                //    #endif
                //}

                int adjustedLMR = LMRLevel;
                //if (isInCheck /*|| isPV*/) {
                //    // Reduce less when in PV node / when giving check / when in check
                //    adjustedLMR_x2 -= 1;
                //}
                //if (isPV) {
                //    adjustedLMR_x2 -= 1;
                //}
                /*if (isInCheck && isPV) {
                    adjustedLMR -= 1;
                }*/
                /*if (moveEvaluations[moveIndex].score < 0 && (adjustedLMR < 3)) {
                    Does not work
                    adjustedLMR += 1;
                }*/
                

                // if (bestKeptCount > 5 && (adjustedLMR < 3)) {    // Good : 31.35 +/- 35.93,  200 games
                // if (bestKeptCount > 8 && (adjustedLMR < 3)) {    // Better : 34.86 +/- 44.83,  100 games compared to 5
                /*if (bestKeptCount > 12 && (adjustedLMR < 3)) {      // Even Better : 33.11 +/- 34.71,  200 games compared to 8
                    adjustedLMR += 1;
                }*/
                // 17.39 +/- 35.76, nElo: 23.53 +/- 48.15 LOS: 83.09 % Games: 200
                /*if (rootDistance != 0) {
                    if (bestKeptCount > 8+rootDistance && (adjustedLMR < 3)) {
                        adjustedLMR += 1;
                    }
                }*/

                // Does not currently work
                /*if (isPV) {
                    if (bestKeptCount > 14 && (adjustedLMR < 3)) {      // Even Better : 33.11 +/- 34.71,  200 games compared to 8
                        adjustedLMR += 1;
                    }
                } else {
                    if (bestKeptCount > 11 && (adjustedLMR < 3)) {      // Even Better : 33.11 +/- 34.71,  200 games compared to 8
                        adjustedLMR += 1;
                    }
                }*/
                // Does not currently work
                /*if (rootDistance >= 1) {
                    if (bestKeptCount > 12 && (adjustedLMR < 3)) {
                        adjustedLMR += 1;
                    }
                } else {
                    if (bestKeptCount > 18 && (adjustedLMR < 3)) {
                        adjustedLMR += 1;
                    }
                }*/

                /*if (ttStability > 0 && (adjustedLMR < 3)) {
                    adjustedLMR += 1;
                }*/

                /*else if (bestKeptCount == 0 && (adjustedLMR < 3)) 0{ // (adjustedLMR > 1)) {
                    // Does not work
                    adjustedLMR -= 1;
                }*/

                // 20.87 +/- 53.93 100 games
                //if (/*!isPV) {*/ rootDistance != 0) {
                //    if (bestKeptCount > /*10*/12 /*&& (adjustedLMR < 3)*/) {
                //        adjustedLMR_x2 += 1;
                //    }
                //}

                // if (isRefutationMoveCapture /*&& (adjustedLMR < 3)*/) {
                //     adjustedLMR += 1;
                // }

                /*if (correctionAmount > 50 || (isPV && correctionAmount > 100)) {
                    adjustedLMR_x2 -= 1;
                }*/

                /*if (bestKeptCount > 14) {
                    adjustedLMR += 1;
                }*/

                //  22.62 +/- 28.94, 200 games
                if (isExpectedCutNode && isRefutationMoveCapture && (adjustedLMR < 3)) {
                    adjustedLMR += 1;
                    // std::cout << depth << " " << adjustedLMR << "\n";
                }
                
                nodeDepth -= std::clamp(adjustedLMR, 0, nodeDepth-1);
                
                
                // Test on null window if score > alpha
                score = -PVSearch(board, nodeDepth, -alpha-1, -alpha, remainingSearchExtensions, remainingHorizonExtensions, move);
                // if (score == -INFINITE_SCORE) { return INFINITE_SCORE; }

                // If score is within the window and windows is not null
                // Do a full research without reduction
                if (alpha < score && beta - alpha > 1) {
                    /*if (depth >= currentDepth-2) {
                        printSpaces(depth);
                        printf("RESEARCH\n");
                    }*/

                    #if MESURE_LEVEL >= ALL_MESURE
                    LMRResearchCount += 1;
                    #endif
                    
                    // Research at full depth
                    nodeDepth = depth - 1;
                    score = -PVSearch(board, nodeDepth, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions, move);
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

                // Enough to gain
                if (!board.isCapture(move)) {
                    addHistoryBonus(whiteTurn, move, depth, 6*2);
                    addKillerMove(board, move);
                    addCounterMove(lastMove, move, whiteTurn);

                    for (int j = 0 ; j < moveIndex ; j++) {
                        Move quietMove = moves[j];
                        if (!board.isCapture(quietMove)) {
                            addHistoryMalus(whiteTurn, quietMove, depth, 1*2);
                        } /*else {
                            addCaptureHistoryMalus(whiteTurn, quietMove, depth, board.getAt(quietMove.startSquare).type, board.getAt(quietMove.endSquare).type);
                        }*/
                    }
                } /*else {
                    addCaptureHistoryBonus(whiteTurn, move, depth, board.getAt(move.startSquare).type, board.getAt(move.endSquare).type);

                    for (int j = 0 ; j < moveIndex ; j++) {
                        Move noisyMove = moves[j];
                        if (board.isCapture(noisyMove)) {
                            addCaptureHistoryMalus(whiteTurn, noisyMove, depth, board.getAt(noisyMove.startSquare).type, board.getAt(noisyMove.endSquare).type);
                        }
                    }
                }*/

                // Current conditions can be improved !
                if (//!hasTT &&
                    !isInCheck &&
                    !board.isCapture(move) &&
                    baseScore < score) {

                    updateCorrection(board, depth, baseScore, score);
                }

                // Fail high, Cut node
                updateTT(currentEntry, board.zobristHash, depth, move, score, CUT_NODE, staticScoreRaw, rootDistance);  //TEST123 updateTT(board, depth, move, score, CUT_NODE); 
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
            } else {
                // TEST : COMMENT THIS
                /*if (!board.isCapture(move)) {
                    addHistoryMalus(whiteTurn, move, depth);
                }*/
            }
        }

        if (!board.isCapture(bestMove)) {
            addHistoryBonus(whiteTurn, bestMove, depth, 3*2);

            // TEST
            for (int j = 0 ; j < moveCount ; j++) {
                Move quietMove = moves[j];

                if (quietMove == bestMove) {
                    continue;
                }

                if (!board.isCapture(quietMove)) {
                    addHistoryMalus(whiteTurn, quietMove, depth, 1*2);
                } /*else {
                    addCaptureHistoryMalus(whiteTurn, quietMove, depth, board.getAt(quietMove.startSquare).type, board.getAt(quietMove.endSquare).type);
                }*/
            }
        } /*else {
            addCaptureHistoryBonus(whiteTurn, bestMove, depth, board.getAt(bestMove.startSquare).type, board.getAt(bestMove.endSquare).type);

            for (int j = 0 ; j < moveCount ; j++) {
                Move noisyMove = moves[j];

                if (noisyMove == bestMove) {
                    continue;
                }

                if (board.isCapture(noisyMove)) {
                    addCaptureHistoryMalus(whiteTurn, noisyMove, depth, board.getAt(noisyMove.startSquare).type, board.getAt(noisyMove.endSquare).type);
                }
            }
        }*/

        #if MESURE_LEVEL >= ALL_MESURE
        bestMovesIndexes[bestMovesIndex] += 1;
        #endif
    }

    // Debug tests
    /*if (bestMove == NO_MOVE && depth >= 5) {
        printSpaces(depth);
        printf("! NO MOVE\n! alpha %d, beta %d, depth %d\n", alpha, beta, depth);
        //throw;
    }
    if (alpha > beta) {
        printf("! ALPHA > BETA\n! alpha %d, beta %d, depth %d\n", alpha, beta, depth);
        throw;
    }
        if (bestMove != NO_MOVE) {    
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
    }
    if (depth >= currentDepth-2) { 
        printSpaces(depth);
        printf("%d TT set %d  beta %d   (%llx)", depth, alpha, beta, board.zobristHash); 
        if (bestMove == NO_MOVE) {
            printf(" NO MOVE");     // shouldn't be possible
        }
        printf("\n");
    }*/

    // Current conditions can be improved !
    // Removing !hasTT condition is  52.51 +/- 50.86, 100 games ! But it is slightly incorrect because baseScore is no longer static eval
    if (//!hasTT &&
        !isInCheck &&
        !(bestMove != NO_MOVE && board.isCapture(bestMove)) &&
        alpha < baseScore) {
        
        updateCorrection(board, depth, baseScore, alpha);
    }

    // Alpha acts as the best score
    updateTT(currentEntry, board.zobristHash, depth, bestMove, alpha, ALL_NODE, staticScoreRaw, rootDistance); //TEST123 updateTT(board, depth, bestMove, alpha, ALL_NODE);
    return alpha;
}


// Returns the best move with the absolute score
MoveResult getBestMove(Board &board, bool verbose=true, bool showBoard=false, bool uciInfos=false) {
    // printf("\n%d   %d\n\n", board.whiteAccumulatorStack.size(), board.blackAccumulatorStack.size()); // DEBUG_
    // printf("%d\n\n", evaluatePosition(board)); // DEBUG_

    isVerbose = verbose;
    isSearchCanceled = false;
    initialPly = board.ply;

    scheduleSearchTimer(std::chrono::milliseconds((int) MAX_BOT_TIME));

    // currentDepth = NORMAL_DEPTH;
    currentDepth = std::max(NORMAL_DEPTH, std::min(currentDepth-3, NORMAL_DEPTH+2));
    currentRepetitionCount = board.repetitionCount;

    auto startTime = std::chrono::system_clock::now();
    auto endTime = std::chrono::system_clock::now();

    // Clears move history ?
    /*for (int color = 0 ; color < 2 ; color++) {
        for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
            for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                moveHistory[color][startSquare][endSquare] = 0;
            }
        }
    }*/
    clearKillerMoves();
    board.cleanPreviousHashes();

    // Reset debug variables
    #if MESURE_LEVEL >= LOW_MESURE
    nodeCount = 1;
    #endif
    #if MESURE_LEVEL >= ALL_MESURE
    evaluationCounter = 0;
    extensionCount = 0;
    // PVHitCount = 0;
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
    
    elapsedTime = 0.0f;

    MoveResult bestResult = {};
    MoveResult lastBestResult = {};

    Board pvBoard;

    while (elapsedTime*2.0f < DEFAULT_BOT_TIME && !isSearchCanceled && currentDepth < MAX_DEPTH-1) {
        if (verbose) {
            printf("- Depth = %d\n", currentDepth);
        }
        if (uciInfos) {
            printf("info depth %d\n", currentDepth);
        }

        // Decays move history
        // decayMoveHistory();
        // Clears move history ???
        /*for (int color = 0 ; color < 2 ; color++) {
            for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
                for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                    moveHistory[color][startSquare][endSquare] = 0;
                }
            }
        }*/

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

            pvBoard = board.copy();
            int pvDepth = currentDepth;
            int pvScore = bestResult.score;

            Move pvMove = bestResult.move;
            while (pvDepth > 1) {
                if (pvBoard.whiteTurn) {
                    updateTT_PV(pvBoard, pvDepth, pvMove, pvScore);
                } else {
                    updateTT_PV(pvBoard, pvDepth, pvMove, -pvScore);
                }
                // updateTT_PV(pvBoard);
                if (pvMove == NO_MOVE) {
                    // printf("No Move !\n");
                    break;
                }
                principalVariation.push_back(pvMove);

                pvDepth -= 1;
                pvBoard.playMove(pvMove);
                /*if (showBoard) {
                    printBoard(pvBoard);
                    printf("  ----\n");
                }*/

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

        currentResult = bestResult;
        #if MESURE_LEVEL >= LOW_MESURE
        kNPS = std::lround((float) nodeCount / elapsedTime);
        #endif
    }
    currentDepth -= 1;

    board.cleanPreviousHashes();    // Should not be called before the end and the after the start of getBestMove

    stopSearchTimer();

    if (verbose) {
        #if MESURE_LEVEL >= LOW_MESURE
        printf("| Nodes : %d   (%d KNps)\n", nodeCount, kNPS);
        #endif
        #if MESURE_LEVEL >= ALL_MESURE
        printf("| Evaluations : %d\n", evaluationCounter);
        printf("| Extensions : %d\n", extensionCount);
        // printf("| PV hits : %d\n", PVHitCount);
        printf("| TT Hit : %d\n", TTHitCount);
        printf("| NMP : %d, Successes : %d\n", NMPCount, NMPPruneCount);
        printf("| LMR : %d, Fails : %d\n", LMRCount, LMRResearchCount);
        // printf("| TT Hit : %d, Collisions : %d\n", TTHitCount, TTCollisionCount);
        #endif
        printf("| Bot took %f milliseconds\n", elapsedTime);
        // printf("  Current pawn key : %llx", pawnCorrectionKey(board));
        std::cout.flush();
    }

    if (!board.whiteTurn) {
        bestResult.score = -bestResult.score;
    }

    // std::cout << "REFRESHES : " << refreshes << std::endl;  // TESTEEE

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

    // Shifts the killer moves, only keep the first
    /*for (int i = 0 ; i < MAX_DEPTH-1 ; i++) {
        killerMoves[i][0] = killerMoves[i+1][0];
        killerMoves[i][1] = killerMoves[i+1][1];
    }*/ // Does not currently work
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

    for (int color = 0 ; color < 2 ; color++) {
        for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
            for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                moveHistory[color][startSquare][endSquare] = 0;
            }
        }
    }

    /*for (int color = 0 ; color < 2 ; color++) {
        for (int capturingType = PAWN ; capturingType <= KING ; capturingType++) {
            for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                for (int capturedType = PAWN ; capturedType <= QUEEN ; capturedType++) {
                    captureMoveHistory[color][capturingType][endSquare][capturedType] = 0; // captureMoveHistoryValueFactor*(piecesStandardValue[capturedType] - piecesStandardValue[capturingType]) / 4;
                }
            }
        }
    }*/

    // TESTFFF
    for (int color = 0 ; color < 2 ; color++) {
        for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
            for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
                counterMoves[color][startSquare][endSquare] = NO_MOVE;
            }
        }
    }

    for (int color = 0 ; color < 2 ; color++) {
        for (int i = 0 ; i < PAWN_CORRHIST_SIZE ; i++) {
            pawnCorrectionHistory[color][i] = 0;
        }
    }
    for (int color = 0 ; color < 2 ; color++) {
        for (int i = 0 ; i < MATERIAL_CORRHIST_SIZE ; i++) {
            materialCorrectionHistory[color][i] = 0;
        }
    }
    /*for (int color = 0 ; color < 2 ; color++) {
        for (Square s1 = 0 ; s1 < 64 ; s1++) {
            for (Square s2 = 0 ; s2 < 64 ; s2++) {
                kingsCorrectionHistory[color][s1][s2] = 0;
            }
        }
    }*/

    clearTT();
}

// For debugging - Prints some info about the status of the Transposition Table
void printTTInfos() {
    int entriesCount = 0;
    int allNodeCount = 0;
    int cutNodeCount = 0;
    int PVNodeCount = 0;
    int depthSum = 0;
    for (const TTEntry &entry : transpositionTable) {
        if (entry.nodeType != NO_NODE) {
            entriesCount += 1;
            depthSum += entry.depth; // - (halfMoveTick - entry.creationTick);

            if (entry.nodeType == ALL_NODE) {
                allNodeCount += 1;
            } else if (entry.nodeType == CUT_NODE) {
                cutNodeCount += 1;
            } else if (entry.nodeType == PV_NODE) {
                PVNodeCount += 1;
            }
        }
    }

    float averageDepth = ((float) depthSum) / (float) entriesCount;

    printf("-- Transposition Table infos : --\n");
    printf("  Capacity : %d/%d\n", entriesCount, TTSize);
    printf("  Size : %d/%d Mo\n", (entriesCount * sizeof(TTEntry)) / 1000000, (TTSize * sizeof(TTEntry)) / 1000000);
    printf("  Average depth : %f\n", averageDepth);

    printf("  All nodes : %d\n", allNodeCount);
    printf("  Cut nodes : %d\n", cutNodeCount);
    printf("  PV nodes : %d\n", PVNodeCount);
}
void printMoveHistoryInfos() {
    printf("-- Move History infos : --\n");

    int * maximumValuesBlackPtr[3] = {
        &moveHistory[BLACK][0][0],
        &moveHistory[BLACK][0][0],
        &moveHistory[BLACK][0][0]
    };
    int * maximumValuesWhitePtr[3] = {
        &moveHistory[WHITE][0][0],
        &moveHistory[WHITE][0][0],
        &moveHistory[WHITE][0][0]
    };

    for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
        for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
            int * currentPtr = &moveHistory[BLACK][startSquare][endSquare];

            if (*currentPtr > *maximumValuesBlackPtr[0]) {
                maximumValuesBlackPtr[2] = maximumValuesBlackPtr[1];
                maximumValuesBlackPtr[1] = maximumValuesBlackPtr[0];
                maximumValuesBlackPtr[0] = currentPtr;
            }
        }
    }
    for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
        for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
            int * currentPtr = &moveHistory[WHITE][startSquare][endSquare];

            if (*currentPtr > *maximumValuesWhitePtr[0]) {
                maximumValuesWhitePtr[2] = maximumValuesWhitePtr[1];
                maximumValuesWhitePtr[1] = maximumValuesWhitePtr[0];
                maximumValuesWhitePtr[0] = currentPtr;
            }
        }
    }

    printf("  Maximum value black :\n");
    for (int i = 0 ; i < 3 ; i++) {
        char offset = maximumValuesBlackPtr[i] - &moveHistory[BLACK][0][0];
        printf("    %d : ", *maximumValuesBlackPtr[i]);
        Move move = {(char) (offset / 64), (char) (offset % 64)};
        printMove(move);
    }
    printf("  Maximum value white :\n");
    for (int i = 0 ; i < 3 ; i++) {
        char offset = maximumValuesWhitePtr[i] - &moveHistory[WHITE][0][0];
        printf("    %d : ", *maximumValuesWhitePtr[i]);
        Move move = {(char) (offset / 64), (char) (offset % 64)};
        printMove(move);
    }

    /*int * maximumValueBlackPtr = std::max_element(&moveHistory[BLACK][0][0], &moveHistory[BLACK][63][63]);
    int * maximumValueWhitePtr = std::max_element(&moveHistory[WHITE][0][0], &moveHistory[WHITE][63][63]);

    int maximumWhiteOffset = maximumValueWhitePtr - &moveHistory[WHITE][0][0];
    Move maximumMoveWhite = {maximumWhiteOffset / 64, maximumWhiteOffset % 64};

    int maximumBlackOffset = maximumValueBlackPtr - &moveHistory[BLACK][0][0];
    Move maximumMoveBlack = {maximumBlackOffset / 64, maximumBlackOffset % 64};

    printf("  Maximum value white : %d\n  ", *maximumValueWhitePtr);
    printMove(maximumMoveWhite);
    printf("  Maximum value black : %d\n  ", *maximumValueBlackPtr);
    printMove(maximumMoveBlack);*/

    // int * minimumValuePtr = std::min_element(&moveHistory[BLACK][0][0], &moveHistory[WHITE][63][63]);
    // printf("Minimum value : %d\n", *minimumValuePtr);*/
}
void printCorrHistInfo() {
    printf("-- Pawn correction history info : --\n");
    printf("  Pawn values black : ");
    for (int i = 0 ; i < PAWN_CORRHIST_SIZE ; i++) {
        printf("%d ", pawnCorrectionHistory[BLACK][i]);
    }
    printf("\n");
    printf("  Pawn values white : ");
    for (int i = 0 ; i < PAWN_CORRHIST_SIZE ; i++) {
        printf("%d ", pawnCorrectionHistory[WHITE][i]);
    }
    printf("\n");

    printf("-- Material correction history info : --\n");
    printf("  Material values black : ");
    for (int i = 0 ; i < MATERIAL_CORRHIST_SIZE ; i++) {
        printf("%d ", materialCorrectionHistory[BLACK][i]);
    }
    printf("\n");
    printf("  Material values white : ");
    for (int i = 0 ; i < MATERIAL_CORRHIST_SIZE ; i++) {
        printf("%d ", materialCorrectionHistory[WHITE][i]);
    }
    printf("\n");
}

void stopSearch() {
    stopSearchTimer();
    isSearchCanceled = true;
}

private:

int initialPly;    // Used to find the fastest checkmate, number of moves since last clock reset
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
