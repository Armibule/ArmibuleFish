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
    
    // short creationTick = 0;    // Tick of creation
    // short bestKeptCount = 0;
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

    int t = std::min(depth, 20);

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

    int t = std::min(depth, 20);

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

    // Detects drawn engames impossible to loose   Not incredibly good, maybe bad...
    int pieces = std::popcount(board.allOccupancy);
    switch (pieces) {
    case 2:
        // KvK is drawn
        return drawScore(board);
    // Does not seem to improve
    case 3:
        if (board.colorBB[WHITE][KNIGHT] | 
            board.colorBB[BLACK][KNIGHT] |
            board.colorBB[WHITE][BISHOP] |
            board.colorBB[BLACK][BISHOP]) {
            // KBvK, KKvK, are drawn
            return drawScore(board);
        }
        break;
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

    // TODO : Test if it improves ? -> looks ok
    if (board.isInCheck(board.whiteTurn)) {
        score -= checkValue;
    }

    score = applyCorrection(board, score);

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
inline void updateTT(TTEntry &currentEntry, uint64_t zobristHash, int depth, const Move &bestMove, int score, NodeType nodeType) {
    if (currentEntry.nodeType == NO_NODE || 
        currentEntry.depth <= depth /*||
        // If it makes the bounds tighter (bad)
        (
            relativeDepth == depth &&
            (
                (nodeType==CUT_NODE && currentEntry->nodeType == CUT_NODE && score > currentEntry->score) || 
                (nodeType==ALL_NODE && currentEntry->nodeType == ALL_NODE && score < currentEntry->score)
            )
        )*/) {
        /*if (currentEntry.bestMove == bestMove) {
            currentEntry.bestKeptCount += 1;
        } else {
            currentEntry.bestKeptCount = 0;
        }*/

        currentEntry.zobristHash = zobristHash;
        currentEntry.depth = depth;
        currentEntry.bestMove = bestMove;
        currentEntry.score = score;
        currentEntry.nodeType = nodeType;
        // currentEntry.creationTick = halfMoveTick;
    }
}
inline void updateTT(const Board &board, int depth, const Move &bestMove, int score, NodeType nodeType) {
    updateTT(transpositionTable[getTTIndex(board)], board.zobristHash, depth, bestMove, score, nodeType);
}
// Use with PV nodes
// Don't forget to use relative scores !
inline void updateTT_PV(const Board &board, int depth, const Move &bestMove, int score) {
    transpositionTable[getTTIndex(board)] = {board.zobristHash, depth, bestMove, score, PV_NODE/*, (short) halfMoveTick*/};
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
inline void addHistoryBonus(bool whiteTurn, const Move &move, int depth) {
    /*moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::min(
        moveHistory[whiteTurn][move.startSquare][move.endSquare] + (depth*depth), 
        moveHistoryMaxValue
    );*/

    //  Lerps value (better)
    int t = std::min(depth, 20);
    int &history = moveHistory[whiteTurn][move.startSquare][move.endSquare];
    history = std::min(
        (history*(256 - t) + moveHistoryMaxValue*t) / 256, 
        moveHistoryMaxValue
    );
}
inline void addHistoryMalus(bool whiteTurn, const Move &move, int depth) {
    /*moveHistory[whiteTurn][move.startSquare][move.endSquare] = std::max(
        moveHistory[whiteTurn][move.startSquare][move.endSquare] - (depth*depth), 
        -moveHistoryMaxValue
    );*/

    //  Lerps value (better)
    int t = std::min(depth, 20);
    int &history = moveHistory[whiteTurn][move.startSquare][move.endSquare];
    history = std::max(
        (history*(256 - t) - moveHistoryMaxValue*t) / 256, 
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


// Counter Moves Heuristic (Work in progress) currently bad
/*Move counterMoves[64][64] = {};
// Only if the move is quiet
inline void addCounterMove(const Move &lastMove, const Move &move) {
    counterMoves[lastMove.startSquare][lastMove.endSquare] = move;
}*/


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

        PieceType capturedPiece = board.pieces[move.endSquare].type;
        PieceType capturingPiece = board.pieces[move.startSquare].type;
        if (capturedPiece == EMPTY) {
            // En passant
            capturedPiece = PAWN;
        }
        // MVV-LVA
        int value = piecesStandardValue[capturedPiece] - piecesStandardValue[capturingPiece];
        
        // MVV-LVA
        // int value = piecesStandardValue[board.getAt(move.endSquare).type] - piecesStandardValue[board.getAt(move.startSquare).type];

        // To test
        // int value = seeCapture(board, move);

        moveBaseEvaluations[i] = {move, value};

        moveBaseEvaluationsPtr[i] = &moveBaseEvaluations[i];
    }

    // The value used is not color dependant
    // std::sort(moveBaseEvaluations, moveBaseEvaluations + moveCount, moveResultCompareDecreasing);
    std::sort(moveBaseEvaluationsPtr, moveBaseEvaluationsPtr + moveCount, moveResultCompareDecreasingPtr);

    for (int i = 0 ; i < moveCount ; i++) {
        MoveResult baseMoveResult = *moveBaseEvaluationsPtr[i]; //moveBaseEvaluations[i];
        /*if (baseMoveResult.score < 0) { break; }*/
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
bool inNMP = false;


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
    for (int capturingType = PAWN ; capturingType <= QUEEN ; capturingType++) {
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

int PVSearch(Board &board, int depth=NORMAL_DEPTH, int alpha=-INFINITE_SCORE, int beta=INFINITE_SCORE, int remainingSearchExtensions=MAX_SEARCH_EXTENSION, int remainingHorizonExtensions=MAX_HORIZON_EXTENSION/*, const Move &lastMove=NO_MOVE*/) {
    if (isSearchCanceled) {
        // Worst score possible for the pervious layer of minmax
        // So this move will not be played
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

    // The handling doesn't seem right ?
    if (board.state != NEUTRAL || board.repetitionCount > currentRepetitionCount) {
        return evaluatePosition(board);
    }

    // Check if the position is present in the transposition table
    Move refutationMove = NO_MOVE;
    bool isRefutationMoveCapture = false;
    bool isExpectedCutNode = false;
    int baseScore;
    bool hasTT = false;
    TTEntry &currentEntry = transpositionTable[getTTIndex(board)];
    // short bestKeptCount = 0;
    if (currentEntry.nodeType != NO_NODE && currentEntry.zobristHash == board.zobristHash) {
        // (currentEntry.depth > depth) { does not currently work
        if (currentEntry.depth >= depth) {
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
                //if (TTDepth > depth) {
                    // The score is exact
                    return currentEntry.score;
                //}
                
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
                // TEST
                // Score is upper bound (relatively)
                // if (currentEntry.score < alpha) { // Almost passes
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
        isExpectedCutNode = currentEntry.nodeType == CUT_NODE;
        
        refutationMove = currentEntry.bestMove;
        baseScore = currentEntry.score;

        isRefutationMoveCapture = board.isCapture(refutationMove);
        // bestKeptCount = currentEntry.bestKeptCount;

        hasTT = true;
    } else {
        baseScore = evaluatePosition(board);
    }

    std::vector<Move> moves;
    board.getAllMoves(moves);

    const int moveCount = moves.size();

    if (moveCount == 0) {
        // This is a leaf node, will only be reached after null moves
        return baseScore;
    }

    bool isInCheck = board.isInCheck(whiteTurn);

    // Reverse futility pruning
    if (!isInCheck && 
        (baseScore > beta + REVERSE_FUTILITY_MARGIN*depth) && 
        !isPV &&
        !isRefutationMoveCapture) {
        // Fail high
        return baseScore;
    }

    // TODO : TEST SEARCH EXTENSIONS !
    makeSearchExtensions(board, isInCheck, depth, remainingSearchExtensions, remainingHorizonExtensions, moveCount);

    // Null Move Pruning
    if (depth < currentDepth-1 && 
        depth > NullMovePruningReduction && 
        // Avoids zugzwangs
        (std::popcount(board.occupencies[whiteTurn]) > 1 + std::popcount(board.colorBB[whiteTurn][PAWN])) && // std::popcount(board.allOccupancy) > 6 && // std::popcount(board.allOccupancy) > 9 && // std::popcount(board.allOccupancy) > 5 && // (std::popcount(board.allOccupancy) > board.whitePieces[PAWN] + board.blackPieces[PAWN] + 2) && //  TO TEST !!!! std::popcount(board.allOccupancy) > 9 &&
        !isPV &&
        // Try only if the position looks good enough
        (baseScore + NMPRejectMargin >= beta) &&
        !isInCheck &&
        !inNMP) {

        inNMP = true;

        #if MESURE_LEVEL >= ALL_MESURE
        NMPCount += 1;
        #endif
        
        board.playNullMove();
        int nullSearchScore = -PVSearch(board, depth - NullMovePruningReduction, -beta, -beta + 1, remainingSearchExtensions, remainingHorizonExtensions/*, NO_MOVE*/);
        board.undoNullMove();

        inNMP = false;

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

    // Removing condition = crazy boost
    //if (true || depth >= 1) {

    // Move ordering
    MoveResult moveEvaluations[moveCount] = {};

    Move killerMove1 = killerMoves[board.ply - initialPly][0];
    Move killerMove2 = killerMoves[board.ply - initialPly][1];
    /*Move counterMove;
    if (lastMove == NO_MOVE) {
        counterMove = NO_MOVE;
    } else {
        counterMove = counterMoves[lastMove.startSquare][lastMove.endSquare];
    }*/

    const int refutationMoveBonus = 10000;
    const int promotionBonusQueen = 600;
    const int captureBonus = 400;
    const int killerMove1Bonus = 120;
    const int killerMove2Bonus = 80;
    // const int counterMoveBonus = 20;

    for (int i = 0 ; i < moveCount ; i++) {
        int value;
        Move move = moves[i];

        if (move == refutationMove) {
            value = refutationMoveBonus;
        } else {
            bool isCapture = board.isCapture(move);
            // PieceType capturedPiece = board.pieces[move.endSquare].type;
            // PieceType capturingPiece = board.pieces[move.startSquare].type;

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
                    value += captureBonus;
                }

                // value += captureMoveHistory[whiteTurn][capturingPiece][move.endSquare][capturedPiece]/captureMoveHistoryValueFactor;
            } else {
                if (move == killerMove1) {
                    value = killerMove1Bonus;
                } else if (move == killerMove2) {
                    value = killerMove2Bonus;
                } else {
                    value = moveHistory[whiteTurn][move.startSquare][move.endSquare]/moveHistoryValueFactor;

                    /*if (move == counterMove) {
                        value += counterMoveBonus;
                    }*/
                }
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
        for (const Move &move : moves) {
        // for (int moveIndex = 0 ; moveIndex < moveCount ; moveIndex++) {
            // Move move = moves[moveIndex];
            bool isCapture = board.isCapture(move);

            // Could benefit from lazy evaluation -> less frquent Accumulator updates
            UnmakeMoveInfo info = board.playMove(move, false);
            
            // Futility pruning, to improve
            if (!isInCheck && 
                (baseScore + FUTILITY_MARGIN < alpha) && 
                !isCapture &&
                !isPV &&
                !board.isInCheck(!whiteTurn)) {
                board.undoMove(move, info, false);
                continue;
            }

            // Updates since it wasn't done by playMove()
            board.updateNNUEAccumulators(info);

            score = -quiescenceSearch(board, -beta, -alpha);
            board.undoMove(move, info);

            if (score >= beta) {
                // Only updates on quiet moves
                if (!isCapture) {
                    addHistoryBonus(whiteTurn, move, depth);
                    addKillerMove(board, move);
                    //addCounterMove(lastMove, move);
                } /*else {
                    addCaptureHistoryBonus(whiteTurn, move, depth, board.getAt(move.endSquare).type);
                }*/


                // Depth 1 corrhist test - IN TEST, RESUME WITH -config
                // Currently neutral, slightly bad : does not pass
                /*if (!hasTT &&
                    !isInCheck &&
                    !board.isCapture(move) &&
                    baseScore < score) {
                    // Assumes baseScore = static eval of the position
                    updateCorrection(board, depth, baseScore, score);
                }*/

                // Fail high, Cut node
                updateTT(currentEntry, board.zobristHash, depth, move, score, CUT_NODE); //TEST123 updateTT(board, depth, move, score, CUT_NODE);
                return score;
            }
            if (score > alpha) {
                bestMove = move;
                alpha = score;
            } else {
                if (!isCapture) {
                    addHistoryMalus(whiteTurn, move, depth);
                }
            }

            // TEST
            /*if (!board.isCapture(bestMove)) {
                addHistoryBonus(whiteTurn, bestMove, depth);

                // TEST
                for (int j = 0 ; j < moveCount ; j++) {
                    Move quietMove = moves[j];

                    if (quietMove == bestMove) {
                        continue;
                    }

                    if (!board.isCapture(quietMove)) {
                        addHistoryMalus(whiteTurn, quietMove, depth);
                    }
                }
            }*/
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

        score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions/*, firstMove*/);        
        
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
                addHistoryBonus(whiteTurn, bestMove, depth);
                addKillerMove(board, bestMove);
                // addCounterMove(lastMove, bestMove);
            } /*else {
                addCaptureHistoryBonus(whiteTurn, bestMove, depth, board.getAt(bestMove.startSquare).type, board.getAt(bestMove.endSquare).type);
            }*/

            // Current conditions can be improved !
            if (!hasTT &&
                !isInCheck &&
                !board.isCapture(firstMove) &&
                baseScore < score) {
                // Assumes baseScore = static eval of the position
                updateCorrection(board, depth, baseScore, score);
            }

            updateTT(currentEntry, board.zobristHash, depth, firstMove, score, CUT_NODE); //TEST123 updateTT(board, depth, firstMove, score, CUT_NODE);     // Fail high, Cut node
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
                } else if (currentDepth-depth < 3) {
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
                if (isInCheck || isPV) {
                    // Reduce less when in PV node / when giving check / when in check
                    adjustedLMR -= 1;
                }
                if (isRefutationMoveCapture && (adjustedLMR < 3)) {
                    adjustedLMR += 1;
                }
                nodeDepth -= std::clamp(LMRLevel, 0, nodeDepth-1);
                
                
                // Test on null window if score > alpha
                score = -PVSearch(board, nodeDepth, -alpha-1, -alpha, remainingSearchExtensions, remainingHorizonExtensions/*, move*/);
                
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
                    score = -PVSearch(board, nodeDepth, -beta, -alpha, remainingSearchExtensions, remainingHorizonExtensions/*, move*/);
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
                    addHistoryBonus(whiteTurn, move, depth);
                    addKillerMove(board, move);
                    // addCounterMove(lastMove, move);

                    for (int j = 0 ; j < moveIndex ; j++) {
                        Move quietMove = moves[j];
                        if (!board.isCapture(quietMove)) {
                            addHistoryMalus(whiteTurn, quietMove, depth);
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
                if (!hasTT &&
                    !isInCheck &&
                    !board.isCapture(move) &&
                    baseScore < score) {
                    // Assumes baseScore = static eval of the position
                    updateCorrection(board, depth, baseScore, score);
                }

                // Fail high, Cut node
                updateTT(currentEntry, board.zobristHash, depth, move, score, CUT_NODE);  //TEST123 updateTT(board, depth, move, score, CUT_NODE); 
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
            addHistoryBonus(whiteTurn, bestMove, depth);

            // TEST
            for (int j = 0 ; j < moveCount ; j++) {
                Move quietMove = moves[j];

                if (quietMove == bestMove) {
                    continue;
                }

                if (!board.isCapture(quietMove)) {
                    addHistoryMalus(whiteTurn, quietMove, depth);
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
    if (!hasTT &&
        !isInCheck &&
        !(bestMove != NO_MOVE && board.isCapture(bestMove)) &&
        alpha < baseScore) {
        // Assumes baseScore = static eval of the position
        updateCorrection(board, depth, baseScore, alpha);
    }

    // Alpha acts as the best score
    updateTT(currentEntry, board.zobristHash, depth, bestMove, alpha, ALL_NODE); //TEST123 updateTT(board, depth, bestMove, alpha, ALL_NODE);
    return alpha;
}


// Returns the best move with the absolute score
MoveResult getBestMove(Board &board, bool verbose=true, bool showBoard=false, bool uciInfos=false) {
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

    /*for (Square startSquare = 0 ; startSquare < 64 ; startSquare++) {
        for (Square endSquare = 0 ; endSquare < 64 ; endSquare++) {
            counterMoves[startSquare][endSquare] = NO_MOVE;
        }
    }*/

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
