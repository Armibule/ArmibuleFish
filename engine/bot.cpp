#include "botConstants.cpp"
#include "board.cpp"
#include <unordered_map>
#include <math.h>
#include <csignal>
#include <signal.h>
#include <thread>
#include <atomic>
#include <chrono>


struct MoveResult {
    Move move = NO_MOVE;
    int score = 0;
};

enum NodeType {
    NO_NODE,    // This entry is empty
    PV_NODE,    // Best moves, exact score
    CUT_NODE,   // Move which are "too good", lower bound score (relatively)
    ALL_NODE    // Bad moves, upper bound score (relatively)
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

inline int lerpScore(int openingScore, int endgameScore, GamePhase phase) {
    return (openingScore * (256 - phase) + endgameScore * phase) / 256;
}


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

    #if MESURE_LEVEL >= ALL_MESURE
    evaluationCounter += 1;
    #endif

    int score = 0;

    // TODO : calibrate
    int mobilityPoints = 0;

    // Used for pawnProtectsBonus, negative if black has more
    int pawnProtecting = 0;

    uint64_t attacksMaskColor[2] = {0ULL, 0ULL};

    uint64_t allOccupency = board.allOccupancy;

    uint64_t blackPawnOccupency = board.colorBB[BLACK][PAWN];
    uint64_t blackBishopOccupency = board.colorBB[BLACK][BISHOP];
    uint64_t blackKnightOccupency = board.colorBB[BLACK][KNIGHT];
    uint64_t blackRookOccupency = board.colorBB[BLACK][ROOK];
    uint64_t blackQueenOccupency = board.colorBB[BLACK][QUEEN];

    uint64_t whitePawnOccupency = board.colorBB[WHITE][PAWN];
    uint64_t whiteBishopOccupency = board.colorBB[WHITE][BISHOP];
    uint64_t whiteKnightOccupency = board.colorBB[WHITE][KNIGHT];
    uint64_t whiteRookOccupency = board.colorBB[WHITE][ROOK];
    uint64_t whiteQueenOccupency = board.colorBB[WHITE][QUEEN];

    const uint64_t notBlackPieces = !board.occupencies[BLACK];
    const uint64_t notWhitePieces = !board.occupencies[WHITE];

    const uint64_t pawnsOccupency = board.colorBB[WHITE][PAWN] | board.colorBB[BLACK][PAWN];

    while (blackPawnOccupency) {
        Square square = popLastSquare(blackPawnOccupency);
        uint64_t attacksMask = board.attacksMask(square, PAWN, BLACK);
        uint64_t capturesMask = attacksMask & notBlackPieces;
        attacksMaskColor[BLACK] |= attacksMask;
        mobilityPoints -= std::popcount(attacksMask);

        pawnProtecting -= std::popcount(attacksMask & (board.colorBB[BLACK][PAWN] | board.colorBB[BLACK][KNIGHT]));

        int x = squareX(square);
        int y = squareY(square);

        uint64_t isolatedPawnMask = isolatedPawnMasks[x];
        
        if (!(isolatedPawnMask & board.colorBB[BLACK][PAWN])) {
            score += isolatedPawnMalus;
        } else {
            /*BROKEN uint64_t shift = 8 * (y-2);
            if (!(((isolatedPawnMask << shift) >> shift) & board.colorBB[BLACK][PAWN])) {
                score += overextendedPawnMalus;
            }*/
        }
        if (!(passedPawnMasksBlack[y][x] & board.colorBB[WHITE][PAWN])) {
            score -= passedPawnBonuses[y];
        }
    }
    while (blackBishopOccupency) {
        Square square = popLastSquare(blackBishopOccupency);
        uint64_t attacksMask = board.attacksMask(square, BISHOP, BLACK);
        uint64_t capturesMask = attacksMask & notBlackPieces;
        attacksMaskColor[BLACK] |= attacksMask;
        mobilityPoints -= std::min(std::popcount(attacksMask), 8);
    }
    while (blackKnightOccupency) {
        Square square = popLastSquare(blackKnightOccupency);
        uint64_t attacksMask = board.attacksMask(square, KNIGHT, BLACK);
        uint64_t capturesMask = attacksMask & notBlackPieces;
        attacksMaskColor[BLACK] |= attacksMask;
        mobilityPoints -= std::popcount(attacksMask);
    }
    while (blackRookOccupency) {
        Square square = popLastSquare(blackRookOccupency);
        uint64_t attacksMask = board.attacksMask(square, ROOK, BLACK);
        uint64_t capturesMask = attacksMask & notBlackPieces;
        attacksMaskColor[BLACK] |= attacksMask;
        mobilityPoints -= std::min(std::popcount(attacksMask), 8);

        // If defended by a rook or queen
        if (attacksMask & (board.colorBB[BLACK][ROOK] | board.colorBB[BLACK][QUEEN])) {
            score -= rookConnectedBonus;
        }

        if (rookMasks[square] & board.colorBB[WHITE][QUEEN]) {
            score -= rookQueenAlignedBonus;
        } else {
            // Mask of the column without the current rook
            uint64_t columnMask = columnMasks[squareX(square)] ^ bit(square);

            if (columnMask & allOccupency == 0) {
                score -= rookOpenColumnBonus;
            } else if (columnMask & (allOccupency ^ pawnsOccupency)) {
                score -= rookSemiOpenColumnBonus;
            }
            // }
        }
    }
    while (blackQueenOccupency) {
        Square square = popLastSquare(blackQueenOccupency);
        uint64_t attacksMask = board.attacksMask(square, QUEEN, BLACK);
        attacksMaskColor[BLACK] |= attacksMask;
    }
    uint64_t blackKingAttacksMask = board.attacksMask(board.blackKingSquare, KING, BLACK);
    attacksMaskColor[BLACK] |= blackKingAttacksMask;

    while (whitePawnOccupency) {
        Square square = popLastSquare(whitePawnOccupency);
        uint64_t attacksMask = board.attacksMask(square, PAWN, WHITE);
        uint64_t capturesMask = attacksMask & notWhitePieces;
        attacksMaskColor[WHITE] |= attacksMask;
        mobilityPoints += std::popcount(attacksMask);

        pawnProtecting += std::popcount(attacksMask & (board.colorBB[WHITE][PAWN] | board.colorBB[WHITE][KNIGHT]));

        int x = squareX(square);
        int y = squareY(square);

        uint64_t isolatedPawnMask = isolatedPawnMasks[x];
        
        if (!(isolatedPawnMask & board.colorBB[WHITE][PAWN])) {
            score -= isolatedPawnMalus;
        } else {
            /*BROKEN uint64_t shift = 8 * (6-y);
            if (!(((isolatedPawnMask >> shift) << shift) & board.colorBB[WHITE][PAWN])) {
                score -= overextendedPawnMalus;
            }*/
        }
        if (!(passedPawnMasksWhite[y][x] & board.colorBB[BLACK][PAWN])) {
            score += passedPawnBonuses[7 - y];
        }
    }
    while (whiteBishopOccupency) {
        Square square = popLastSquare(whiteBishopOccupency);
        uint64_t attacksMask = board.attacksMask(square, BISHOP, WHITE);
        uint64_t capturesMask = attacksMask & notWhitePieces;
        attacksMaskColor[WHITE] |= attacksMask;
        mobilityPoints += std::min(std::popcount(attacksMask), 8);
    }
    while (whiteKnightOccupency) {
        Square square = popLastSquare(whiteKnightOccupency);
        uint64_t attacksMask = board.attacksMask(square, KNIGHT, WHITE);
        uint64_t capturesMask = attacksMask & notWhitePieces;
        attacksMaskColor[WHITE] |= attacksMask;
        mobilityPoints += std::popcount(attacksMask);
    }
    while (whiteRookOccupency) {
        Square square = popLastSquare(whiteRookOccupency);
        uint64_t attacksMask = board.attacksMask(square, ROOK, WHITE);
        uint64_t capturesMask = attacksMask & notWhitePieces;
        attacksMaskColor[WHITE] |= attacksMask;
        mobilityPoints += std::min(std::popcount(attacksMask), 8);

        // If defended by a rook or queen
        if (attacksMask & (board.colorBB[WHITE][ROOK] | board.colorBB[WHITE][QUEEN])) {
            score += rookConnectedBonus;
        }

        if (rookMasks[square] & board.colorBB[BLACK][QUEEN]) {
            score += rookQueenAlignedBonus;
        } else {
            // Mask of the column without the current rook
            uint64_t columnMask = columnMasks[squareX(square)] ^ bit(square);

            if (columnMask & allOccupency == 0) {
                score += rookOpenColumnBonus;
            } else if (columnMask & (allOccupency ^ pawnsOccupency)) {
                score += rookSemiOpenColumnBonus;
            }
            //}
        }
    }
    while (whiteQueenOccupency) {
        Square square = popLastSquare(whiteQueenOccupency);
        uint64_t attacksMask = board.attacksMask(square, QUEEN, WHITE);
        attacksMaskColor[WHITE] |= attacksMask;
    }
    uint64_t whiteKingAttacksMask = board.attacksMask(board.whiteKingSquare, KING, WHITE);
    attacksMaskColor[WHITE] |= whiteKingAttacksMask;

    blackPawnOccupency = board.colorBB[BLACK][PAWN];
    blackBishopOccupency = board.colorBB[BLACK][BISHOP];
    blackKnightOccupency = board.colorBB[BLACK][KNIGHT];
    blackRookOccupency = board.colorBB[BLACK][ROOK];
    blackQueenOccupency = board.colorBB[BLACK][QUEEN];

    whitePawnOccupency = board.colorBB[WHITE][PAWN];
    whiteBishopOccupency = board.colorBB[WHITE][BISHOP];
    whiteKnightOccupency = board.colorBB[WHITE][KNIGHT];
    whiteRookOccupency = board.colorBB[WHITE][ROOK];
    whiteQueenOccupency = board.colorBB[WHITE][QUEEN];

    // Incrementally updated piece-square tables
    // Tapered eval : lerps the values between opening and endgame
    score += lerpScore(board.pieceSquareScoreOpening, board.pieceSquareScoreEndgame, board.phase);

    score += mobilityPoints * mobilityValue;
    score += pawnProtectsBonus * pawnProtecting;

    // Pawn structure
    uint64_t whitePawns = board.colorBB[WHITE][PAWN];
    for (const uint64_t columnMask : columnMasks) {
        score -= alignedPawnPenalties[std::popcount(whitePawns & columnMask)];
    }
    uint64_t blackPawns = board.colorBB[BLACK][PAWN];
    for (const uint64_t columnMask : columnMasks) {
        score += alignedPawnPenalties[std::popcount(blackPawns & columnMask)];
    }

    // Good to have a bishop pair
    score -= bishopPairBonus * (board.blackPieces[BISHOP] >= 2);
    score += bishopPairBonus * (board.whitePieces[BISHOP] >= 2);

    // Two knights are redundent
    score -= knightPairPenalty * (board.whitePieces[KNIGHT] >= 2);
    score += knightPairPenalty * (board.blackPieces[KNIGHT] >= 2);

    // Two rooks are redundent
    score -= rookPairPenalty * (board.whitePieces[ROOK] >= 2);
    score += rookPairPenalty * (board.blackPieces[ROOK] >= 2);

    const char castlingFlag = board.castlingFlag;
    // Castle availability
    score -= shortCastleBonus * (bool) (castlingFlag & SHORT_CASTLE_BLACK);
    score += shortCastleBonus * (bool) (castlingFlag & SHORT_CASTLE_WHITE);
    
    score -= longCastleBonus * (bool) (castlingFlag & LONG_CASTLE_BLACK);
    score += longCastleBonus * (bool) (castlingFlag & LONG_CASTLE_WHITE);

    // King safety
    // Being in check is bad
    if (board.isInCheck(board.whiteTurn)) {
        if (board.whiteTurn) {
            score -= checkValue;
        } else {
            score += checkValue;
        }
    }

    // Better when protected with pawns
    int whiteKingPawnsCount = std::popcount(whiteKingAttacksMask & board.colorBB[WHITE][PAWN]);
    int blackKingPawnsCount = std::popcount(blackKingAttacksMask & board.colorBB[BLACK][PAWN]);

    score += kingPawnsBonus * (whiteKingPawnsCount - blackKingPawnsCount);

    // Bad to have an open file next to the king
    int whiteKingX = squareX(board.whiteKingSquare);
    int blackKingX = squareX(board.blackKingSquare);

    if (columnMasks[whiteKingX] & pawnsOccupency == 0) {
        score -= kingOpenFilesMalus;
    }
    if (columnMasks[blackKingX] & pawnsOccupency == 0) {
        score += kingOpenFilesMalus;
    }

    if (whiteKingX > 0 && (columnMasks[whiteKingX-1] & pawnsOccupency == 0)) {
        score -= kingOpenFilesMalus;
    }
    if (blackKingX > 0 && (columnMasks[blackKingX-1] & pawnsOccupency == 0)) {
        score += kingOpenFilesMalus;
    }

    if (whiteKingX < 7 && (columnMasks[whiteKingX+1] & pawnsOccupency == 0)) {
        score -= kingOpenFilesMalus;
    }
    if (blackKingX < 7 && (columnMasks[blackKingX+1] & pawnsOccupency == 0)) {
        score += kingOpenFilesMalus;
    }

    if (board.phase < ENDGAME_THRESHOLD) {
        // Virtual mobility is bad until engame
        uint64_t whiteVitrualMobility = board.attacksMask(board.whiteKingSquare, QUEEN, WHITE);
        uint64_t blackVitrualMobility = board.attacksMask(board.blackKingSquare, QUEEN, BLACK);

        score += kingVirtualMobilityMalus * (std::popcount(blackVitrualMobility) - std::popcount(whiteVitrualMobility));
    }
    
    //SEEMS BAD... 
    /*if (TEST_VAR) {
        int whiteAttackedKingZoneCount = std::popcount(whiteKingZone & capturesMaskColor[BLACK]);
        int blackAttackedKingZoneCount = std::popcount(blackKingZone & capturesMaskColor[WHITE]);

        score += attackedKingZoneMalus * (blackAttackedKingZoneCount - whiteAttackedKingZoneCount);
    }*/

    // Relative score
    if (!board.whiteTurn) {
        score = -score;
    }

    // From here everything has to be relative

    // Being able to play is good
    score += turnBonus;
    return score;
}

// Transposition table
// TODO : Finish, Check efficiency, Check correctness, Use more
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
void makeSearchExtensions(Board &board, int &depth, int &remainingSearchExtensions, std::vector<Move> &moves) {
    if (remainingSearchExtensions <= 0) {
        return;
    }
    
    if (depth == 1 && moves.size() == 1) {
        depth += 1;
        remainingSearchExtensions -= 1;
        return;
    }
    if (depth == 1) {
        // Extends if in check
        if (board.isInCheck(board.whiteTurn)) {
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

    if (board.state != NEUTRAL) {
        int score = evaluatePosition(board);
        // This is a leaf node, end of the game
        updateTT(board, depth, bestMove, score, ALL_NODE);
        return score;
    }

    // Check if the position is present in the transposition table
    Move refutationMove = NO_MOVE;
    int baseScore;
    TTEntry entry = transpositionTable[getTTIndex(board)];
    if (entry.nodeType != NO_NODE && entry.zobristHash == board.zobristHash) {
        if (relativeTTDepth(entry) >= depth) {
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  IS STORED\n", depth, alpha, beta); 
            }*/

            #if MESURE_LEVEL >= ALL_MESURE
            TTHitCount += 1;
            #endif

            switch (entry.nodeType) {
            case PV_NODE:
                // The score is exact
                return entry.score;
            case CUT_NODE:
                // Score is lower bound (relatively)
                if (entry.score >= beta) {
                    // Fail high
                    return entry.score;
                }
                if (entry.score > alpha) {
                    alpha = entry.score-1;
                }
                break;
            case ALL_NODE:
                // Score is upper bound (relatively)
                if (entry.score <= alpha) {
                    // Fail low
                    return entry.score;
                }
                if (entry.score < beta) {
                    beta = entry.score+1;
                }
                break;
            }

            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("  -> alpha %d, beta %d\n", alpha, beta); 
            }*/
        }

        isPV = entry.nodeType == PV_NODE;
        
        refutationMove = entry.bestMove;
        baseScore = entry.score;
    } else {
        baseScore = evaluatePosition(board);
    }    

    // Null Move Pruning, should always be before move generation
    if (depth < currentDepth-1 && 
        depth > NullMovePruningReduction && 
        board.phase < ENDGAME_THRESHOLD && 
        !isPV &&
        // Try only if the position looks good enought
        (baseScore + NMPRejectMargin >= beta) &&
        !board.isInCheck(whiteTurn)) {

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

    /*if (TEST_VAR) {
        makeSearchExtensions(board, depth, remainingSearchExtensions, moves);
    }*/

    if (depth > 1) {        
        // Move ordering
        MoveResult moveEvaluations[moveCount] = {};

        int refutationMoveBonus;
        int pvNodeBonus;
        int cutNodeBonus;

        pvNodeBonus = 4000;
        refutationMoveBonus = 2000;
        cutNodeBonus = 100;

        for (int i = 0 ; i < moveCount ; i++) {
            int value;

            if (moves[i] == refutationMove) {
                value = entry.score + refutationMoveBonus;
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
                    value = -evaluatePosition(board);
                }

                board.undoMove(moves[i], info);
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
        bool futilityPruningEnabled = !board.isInCheck(whiteTurn);
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
        // Initial full search of expected best move
        Move firstMove = moves[0];
        UnmakeMoveInfo info = board.playMove(firstMove);

        score = -PVSearch(board, depth - 1, -beta, -alpha, remainingSearchExtensions);        
        board.undoMove(firstMove, info);

        // Fallback move in case none increase alpha (usefull in iterative deepening)
        bestMove = firstMove;

        if (score >= beta) {
            /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d Cut %d beta %d   (%llx)\n", depth, score, beta, board.zobristHash); 
            }*/

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
            if (depth - 1 - LMRLevel > 1 && 
                LMRLevel < 1 && 
                depth <= maxLMRDepth && 
                moveCount >= LMR_MOVE_NUMBER) 
            {
                LMRLevel += 1;
            }

            Move move = moves[moveIndex];
            UnmakeMoveInfo info = board.playMove(move);

            int nodeDepth = depth - 1;

            if (board.state != NEUTRAL) {
                score = -evaluatePosition(board);
            } else {
                // only reduce when not in check
                if (!board.isInCheck(whiteTurn)) {
                    nodeDepth = depth - 1 - LMRLevel;
                }
                #if MESURE_LEVEL >= ALL_MESURE
                if (LMRLevel > 0) {
                    LMRCount += 1;
                }
                #endif
                
                // Test on null window if score > alpha
                score = -PVSearch(board, nodeDepth, -alpha-1, -alpha, remainingSearchExtensions);
                
                // If score is within the window, do a full research
                // TODO : check if we should research when possible cut node or not
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

                // Fail high, Cut node
                updateTT(board, depth, move, score, CUT_NODE); 
                return score;
            }
            if (score > alpha) {
                bestMove = move;
                alpha = score;

                /*if (depth >= currentDepth-2) {
                printSpaces(depth);
                printf("%d  alpha %d, beta %d\n", depth, alpha, beta); 
                }*/
            }
        }
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
MoveResult getBestMove(Board &board, bool verbose=true, bool showBoard=false) {
    isVerbose = verbose;
    isSearchCanceled = false;
    initialPreviousHashesSize = board.previousHashes.size();

    scheduleSearchTimer(std::chrono::milliseconds((int) MAX_BOT_TIME));

    currentDepth = NORMAL_DEPTH;

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

    while (elapsedTime*3.0f < DEFAULT_BOT_TIME && !isSearchCanceled) {
        if (verbose) {
            printf("- Depth = %d\n", currentDepth);
        }

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
        #endif

        lastBestResult = bestResult;
        PVSearch(board, currentDepth);

        TTEntry rootEntry = transpositionTable[getTTIndex(board)];
        bestResult = {rootEntry.bestMove, rootEntry.score};

        if (bestResult.move == NO_MOVE) {
            printf("NULL MOVE !\nEntry: ");
            printTTEntry(rootEntry);
            std::cout.flush();
            throw;
        }

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

        // printf("Root Node: type %d, score %d\n", rootEntry.nodeType, rootEntry.score);
        // printMove(rootEntry.bestMove);

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

        if (bestResult.move == NO_MOVE) {
            return bestResult;
        }

        endTime = std::chrono::system_clock::now();
        elapsedTime = (endTime-startTime).count() / 1000000.0f;

        if (verbose) {
            std::cout.flush();
        }

        currentDepth += 1;

        // If we will checkmate and the depth doesn't affect it, no need to search further
        /*if (lastBestResult.score == bestResult.score && (bestResult.score > CHECKMATE_BASE_SCORE - 100 || bestResult.score < -CHECKMATE_BASE_SCORE + 100)) {
            break;
        }*/
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

    return bestResult;
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

    printf("Transposition Table infos :\n");
    printf("Capacity : %d/%d\n", entriesCount, TTSize);
    printf("Size : %d/%d Mo\n", (entriesCount * sizeof(TTEntry)) / 1000000, (TTSize * sizeof(TTEntry)) / 1000000);
    printf("Average relative depth : %f\n", averageRelativeDepth);

    printf("All nodes : %d\n", allNodeCount);
    printf("Cut nodes : %d\n", cutNodeCount);
    printf("PV nodes : %d\n", PVNodeCount);
}

private:

int initialPreviousHashesSize;    // Used to find the fastest checkmate, number of moves since last clock reset
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
