#include "botConstants.cpp"
#include "board.cpp"
#include <unordered_map>
#include <math.h>


// Debug infos
int nodeCount = 1;
int evaluationCounter = 0;
int extensionCount = 0;
int NMPCount = 0;
int PVHitCount = 0;
int TTCollisionCount = 0;
int TTHitCount = 0;



inline float piecesValue(PieceType pieceType, bool isWhite, uint64_t &occupency, float (*pieceValuesPosColor)[PIECE_TYPE_COUNT][64]) {
    float sum = 0.0f;
    while (occupency) {
        Square square = popLastSquare(occupency);
        sum += pieceValuesPosColor[isWhite][pieceType][square];
    }
    return sum;
}

// Use only on the side who can't play
inline float piecesValueAttackAware(PieceType pieceType, bool isWhite, uint64_t occupency, float (*pieceValuesPosColor)[PIECE_TYPE_COUNT][64], uint64_t capturesMaskColor[2]) {
    float sum = 0.0f;
    while (occupency) {
        uint64_t b = popLastBit(occupency);
        Square square = bitToSquare(b);

        float value = pieceValuesPosColor[isWhite][pieceType][square];

        // Is attacked
        if (b & capturesMaskColor[!isWhite]) {
            // Is defended
            if (b & capturesMaskColor[isWhite]) {
                value *= attackedDefendedPenaltyRatio;
            } else {
                value *= attackedPenaltyRatio;
            }
        }
        sum += value;
    }
    return sum;
}




float evaluatePosition(Board &board) {
    switch (board.state) {
    case WHITE_WON:
        return 99999.0f - (float) board.previousHashes.size();  // Hack to go for the fastest checkmate + avoid loops
    case BLACK_WON:
        return -99999.0f + (float) board.previousHashes.size();
    case DRAW:
        return 0.0f;
    }

    if (board.isEvaluationStored) {
        return board.storedEvalutaion;
    }

    evaluationCounter += 1;

    float (*pieceValuesPosColor)[PIECE_TYPE_COUNT][64];

    switch (board.phase)
    {
    case OPENING:
        pieceValuesPosColor = pieceValuesPosOpeningColor;
        break;
    case MIDDLEGAME:
        pieceValuesPosColor = pieceValuesPosMiddlegameColor;
        break;
    case ENDGAME:
        pieceValuesPosColor = pieceValuesPosEndgameColor;
        break;
    }

    float score = 0.0f;

    // TODO : calibrate
    int mobilityPoints = 0;

    // Used for pawnProtectsBonus, negative if black has more
    int pawnProtecting = 0;

    // Square by square method
    uint64_t capturesMaskColor[2] = {0ULL, 0ULL};

    // TODO : REWRITE THE MESS
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

    while (blackPawnOccupency) {
        Square square = popLastSquare(blackPawnOccupency);
        uint64_t capturesMask = board.capturesMask(square, PAWN, BLACK);
        capturesMaskColor[BLACK] |= capturesMask;
        mobilityPoints -= std::popcount(capturesMask);

        pawnProtecting -= std::popcount(colorsPawnAttacks[BLACK][square] & (board.colorBB[BLACK][PAWN] | board.colorBB[BLACK][KNIGHT]));

        int x = squareX(square);
        int y = squareY(square);
        
        if (!(isolatedPawnMasks[x] & board.colorBB[BLACK][PAWN])) {
            score += isolatedPawnMalus;
        }
        if (!(passedPawnMasksBlack[y][x] & board.colorBB[WHITE][PAWN])) {
            score -= passedPawnBonuses[y];
        }
    }
    while (blackBishopOccupency) {
        Square square = popLastSquare(blackBishopOccupency);
        uint64_t capturesMask = board.capturesMask(square, BISHOP, BLACK);
        capturesMaskColor[BLACK] |= capturesMask;
        mobilityPoints -= std::min(std::popcount(capturesMask), 8);
    }
    while (blackKnightOccupency) {
        Square square = popLastSquare(blackKnightOccupency);
        uint64_t capturesMask = board.capturesMask(square, KNIGHT, BLACK);
        capturesMaskColor[BLACK] |= capturesMask;
        mobilityPoints -= std::popcount(capturesMask);
    }
    while (blackRookOccupency) {
        Square square = popLastSquare(blackRookOccupency);
        uint64_t capturesMask = board.capturesMask(square, ROOK, BLACK);
        capturesMaskColor[BLACK] |= capturesMask;
        mobilityPoints -= std::min(std::popcount(capturesMask), 8);

        if (rookMasks[square] & board.colorBB[WHITE][QUEEN]) {
            score -= rookQueenAlignedBonus;
        } else {
            // Mask of the column without the current rook
            uint64_t columnMask = columnMasks[squareX(square)] ^ bit(square);

            if (columnMask & allOccupency == 0) {
                score -= rookOpenColumnBonus;
            } else if (columnMask & (allOccupency ^ (board.colorBB[WHITE][PAWN] | board.colorBB[BLACK][PAWN]))) {
                score -= rookSemiOpenColumnBonus;
            }
        }
    }
    while (blackQueenOccupency) {
        Square square = popLastSquare(blackQueenOccupency);
        uint64_t capturesMask = board.capturesMask(square, QUEEN, BLACK);
        capturesMaskColor[BLACK] |= capturesMask;
    }
    uint64_t blackKingCapturesMask = board.capturesMask(board.blackKingSquare, KING, BLACK);
    capturesMaskColor[BLACK] |= blackKingCapturesMask;
    mobilityPoints -= std::popcount(blackKingCapturesMask);

    while (whitePawnOccupency) {
        Square square = popLastSquare(whitePawnOccupency);
        uint64_t capturesMask = board.capturesMask(square, PAWN, WHITE);
        capturesMaskColor[WHITE] |= capturesMask;
        mobilityPoints += std::popcount(capturesMask);

        pawnProtecting += std::popcount(colorsPawnAttacks[WHITE][square] & (board.colorBB[WHITE][PAWN] | board.colorBB[WHITE][KNIGHT]));

        int x = squareX(square);
        int y = squareY(square);
        
        if (!(isolatedPawnMasks[x] & board.colorBB[WHITE][PAWN])) {
            score -= isolatedPawnMalus;
        }
        if (!(passedPawnMasksWhite[y][x] & board.colorBB[BLACK][PAWN])) {
            score += passedPawnBonuses[7 - y];
        }
    }
    while (whiteBishopOccupency) {
        Square square = popLastSquare(whiteBishopOccupency);
        uint64_t capturesMask = board.capturesMask(square, BISHOP, WHITE);
        capturesMaskColor[WHITE] |= capturesMask;
        mobilityPoints += std::min(std::popcount(capturesMask), 8);
    }
    while (whiteKnightOccupency) {
        Square square = popLastSquare(whiteKnightOccupency);
        uint64_t capturesMask = board.capturesMask(square, KNIGHT, WHITE);
        capturesMaskColor[WHITE] |= capturesMask;
        mobilityPoints += std::popcount(capturesMask);
    }
    while (whiteRookOccupency) {
        Square square = popLastSquare(whiteRookOccupency);
        uint64_t capturesMask = board.capturesMask(square, ROOK, WHITE);
        capturesMaskColor[WHITE] |= capturesMask;
        mobilityPoints += std::min(std::popcount(capturesMask), 8);

        if (rookMasks[square] & board.colorBB[BLACK][QUEEN]) {
            score += rookQueenAlignedBonus;
        } else {
            // Mask of the column without the current rook
            uint64_t columnMask = columnMasks[squareX(square)] ^ bit(square);

            if (columnMask & allOccupency == 0) {
                score += rookOpenColumnBonus;
            } else if (columnMask & (allOccupency ^ (board.colorBB[WHITE][PAWN] | board.colorBB[BLACK][PAWN]))) {
                score += rookSemiOpenColumnBonus;
            }
        }
    }
    while (whiteQueenOccupency) {
        Square square = popLastSquare(whiteQueenOccupency);
        uint64_t capturesMask = board.capturesMask(square, QUEEN, WHITE);
        capturesMaskColor[WHITE] |= capturesMask;
    }
    uint64_t whiteKingCapturesMask = board.capturesMask(board.whiteKingSquare, KING, WHITE);
    capturesMaskColor[WHITE] |= whiteKingCapturesMask;
    mobilityPoints += std::popcount(whiteKingCapturesMask);


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


    if (board.whiteTurn) {
        score += piecesValueAttackAware(PAWN, BLACK, blackPawnOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(BISHOP, BLACK, blackBishopOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(KNIGHT, BLACK, blackKnightOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(ROOK, BLACK, blackRookOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(QUEEN, BLACK, blackQueenOccupency, pieceValuesPosColor, capturesMaskColor);
        
        score += piecesValue(PAWN, WHITE, whitePawnOccupency, pieceValuesPosColor);
        score += piecesValue(BISHOP, WHITE, whiteBishopOccupency, pieceValuesPosColor);
        score += piecesValue(KNIGHT, WHITE, whiteKnightOccupency, pieceValuesPosColor);
        score += piecesValue(ROOK, WHITE, whiteRookOccupency, pieceValuesPosColor);
        score += piecesValue(QUEEN, WHITE, whiteQueenOccupency, pieceValuesPosColor);
    } else {
        score += piecesValue(PAWN, BLACK, blackPawnOccupency, pieceValuesPosColor);
        score += piecesValue(BISHOP, BLACK, blackBishopOccupency, pieceValuesPosColor);
        score += piecesValue(KNIGHT, BLACK, blackKnightOccupency, pieceValuesPosColor);
        score += piecesValue(ROOK, BLACK, blackRookOccupency, pieceValuesPosColor);
        score += piecesValue(QUEEN, BLACK, blackQueenOccupency, pieceValuesPosColor);

        score += piecesValueAttackAware(PAWN, WHITE, whitePawnOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(BISHOP, WHITE, whiteBishopOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(KNIGHT, WHITE, whiteKnightOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(ROOK, WHITE, whiteRookOccupency, pieceValuesPosColor, capturesMaskColor);
        score += piecesValueAttackAware(QUEEN, WHITE, whiteQueenOccupency, pieceValuesPosColor, capturesMaskColor);
    }
    score += pieceValuesPosColor[BLACK][KING][board.blackKingSquare];
    score += pieceValuesPosColor[WHITE][KING][board.whiteKingSquare];


    /*float value;
    while (blackPawnOccupency) {
        Square square = popLastSquare(blackPawnOccupency);
        value = pieceValuesPosColor[BLACK][PAWN][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[WHITE]) {
                if (b & capturesMaskColor[BLACK]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (blackBishopOccupency) {
        Square square = popLastSquare(blackBishopOccupency);
        value = pieceValuesPosColor[BLACK][BISHOP][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[WHITE]) {
                if (b & capturesMaskColor[BLACK]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (blackKnightOccupency) {
        Square square = popLastSquare(blackKnightOccupency);
        value = pieceValuesPosColor[BLACK][KNIGHT][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[WHITE]) {
                if (b & capturesMaskColor[BLACK]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (blackRookOccupency) {
        Square square = popLastSquare(blackRookOccupency);
        value = pieceValuesPosColor[BLACK][ROOK][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[WHITE]) {
                if (b & capturesMaskColor[BLACK]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (blackQueenOccupency) {
        Square square = popLastSquare(blackQueenOccupency);
        value = pieceValuesPosColor[BLACK][QUEEN][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[WHITE]) {
                if (b & capturesMaskColor[BLACK]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    score += pieceValuesPosColor[BLACK][KING][board.blackKingSquare];

    while (whitePawnOccupency) {
        Square square = popLastSquare(whitePawnOccupency);
        value = pieceValuesPosColor[WHITE][PAWN][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);
            
            if (b & capturesMaskColor[BLACK]) {
                if (b & capturesMaskColor[WHITE]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (whiteBishopOccupency) {
        Square square = popLastSquare(whiteBishopOccupency);
        value = pieceValuesPosColor[WHITE][BISHOP][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[BLACK]) {
                if (b & capturesMaskColor[WHITE]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (whiteKnightOccupency) {
        Square square = popLastSquare(whiteKnightOccupency);
        value = pieceValuesPosColor[WHITE][KNIGHT][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[BLACK]) {
                if (b & capturesMaskColor[WHITE]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (whiteRookOccupency) {
        Square square = popLastSquare(whiteRookOccupency);
        value = pieceValuesPosColor[WHITE][ROOK][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[BLACK]) {
                if (b & capturesMaskColor[WHITE]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    while (whiteQueenOccupency) {
        Square square = popLastSquare(whiteQueenOccupency);
        value = pieceValuesPosColor[WHITE][QUEEN][square];

        if (board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[BLACK]) {
                if (b & capturesMaskColor[WHITE]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        score += value;
    }
    score += pieceValuesPosColor[WHITE][KING][board.whiteKingSquare];*/


    /*uint64_t allOccupency = board.allOccupancy;
    int pieceCount = std::popcount(allOccupency);
    
    // TODO : cache in board representation ?
    Piece pieces[pieceCount];
    Square piecesSquares[pieceCount];

    int currentPieceIndex = 0;

    // CHECK CORRECTNESS
    // Bitscan without vector
    while (allOccupency) {
        uint64_t LS1B = allOccupency & (-allOccupency);
        allOccupency ^= LS1B;

        Square square = _lzcnt_u64(LS1B);

        // piece.type shouldn't be EMPTY
        Piece piece = board.getAt(square);

        pieces[currentPieceIndex] = piece;
        piecesSquares[currentPieceIndex] = square;
        currentPieceIndex += 1;

        bool isWhite = piece.isWhite;

        uint64_t capturesMask = board.capturesMask(square, piece.type, isWhite);
        capturesMaskColor[isWhite] |= capturesMask;

        if (piece.type != QUEEN) {
            if (isWhite) {
                mobilityPoints += std::min(std::popcount(capturesMask), 8);
            } else {
                mobilityPoints -= std::min(std::popcount(capturesMask), 8);
            }
        }

        // Piece-specific evalutaion
        switch (piece.type) {
        case ROOK:
            // Rook aligns with enemy queen
            if (rookMasks[square] & board.colorBB[!isWhite][QUEEN]) {
                if (isWhite) {
                    score += rookQueenAlignedBonus;
                } else {
                    score -= rookQueenAlignedBonus;
                }
            } else {
                // Mask of the column without the current rook
                uint64_t columnMask = columnMasks[square % 8u] ^ bit(square);

                if (columnMask & allOccupency == 0) {
                    if (isWhite) {
                        score += rookOpenColumnBonus;
                    } else {
                        score -= rookOpenColumnBonus;
                    }
                } else if (columnMask & (allOccupency ^ (board.colorBB[WHITE][PAWN] | board.colorBB[BLACK][PAWN]))) {
                    if (isWhite) {
                        score += rookSemiOpenColumnBonus;
                    } else {
                        score -= rookSemiOpenColumnBonus;
                    }
                }
            }
            break;
        case PAWN:
            // It is good to have pawns forming chains and defending knights
            if (isWhite) {
                pawnProtecting += std::popcount(colorsPawnAttacks[WHITE][square] & (board.colorBB[WHITE][PAWN] | board.colorBB[WHITE][KNIGHT]));
            } else {
                pawnProtecting -= std::popcount(colorsPawnAttacks[BLACK][square] & (board.colorBB[BLACK][PAWN] | board.colorBB[BLACK][KNIGHT]));
            }

            int x = square % 8u;
            int y = square / 8u;
            
            if (!(isolatedPawnMasks[x] & board.colorBB[isWhite][PAWN])) {
                if (isWhite) {
                    score -= isolatedPawnMalus;
                } else {
                    score += isolatedPawnMalus;
                }
            }
            
            if (isWhite) {
                if (!(passedPawnMasksWhite[y][x] & board.colorBB[BLACK][PAWN])) {
                    score += passedPawnBonuses[7 - y];
                }
            } else {
                if (!(passedPawnMasksBlack[y][x] & board.colorBB[WHITE][PAWN])) {
                    score -= passedPawnBonuses[y];
                }
            }
            break;
        }
    }

    for (int i = 0 ; i < pieceCount ; i++) {
        Piece piece = pieces[i];
        Square square = piecesSquares[i];

        float value = pieceValuesPosColor[piece.isWhite][piece.type][square];

        if (piece.isWhite != board.whiteTurn) {
            uint64_t b = bit(square);

            if (b & capturesMaskColor[!piece.isWhite]) {
                if (b & capturesMaskColor[piece.isWhite]) {
                    value *= attackedDefendedPenaltyRatio;
                } else {
                    value *= attackedPenaltyRatio;
                }
            }  
        }
        
        score += value;
    }*/

    score += mobilityPoints * mobilityValue;
    score += pawnProtectsBonus * pawnProtecting;

    // Pawn structure
    for (const uint64_t columnMask : columnMasks) {
        int whitePawnCount = std::popcount(board.colorBB[WHITE][PAWN] & columnMask);
        int blackPawnCount = std::popcount(board.colorBB[BLACK][PAWN] & columnMask);

        score += alignedPawnPenalties[blackPawnCount] - alignedPawnPenalties[whitePawnCount];
    }

    // Being able to play is good
    if (board.whiteTurn) {
        score += turnBonus;
    } else {
        score -= turnBonus;
    }

    // Good to have a bishop pair
    score += bishopPairBonus * (board.whitePieces[BISHOP] >= 2);
    score -= bishopPairBonus * (board.blackPieces[BISHOP] >= 2);

    // Two knights are redundent
    score -= knightPairPenalty * (board.whitePieces[KNIGHT] >= 2);
    score += knightPairPenalty * (board.blackPieces[KNIGHT] >= 2);

    // Two rooks are redundent
    score -= rookPairPenalty * (board.whitePieces[ROOK] >= 2);
    score += rookPairPenalty * (board.blackPieces[ROOK] >= 2);

    const char castlingFlag = board.castlingFlag;
    // Castle availability
    score += shortCastleBonus * (bool) (castlingFlag & SHORT_CASTLE_WHITE);
    score -= shortCastleBonus * (bool) (castlingFlag & SHORT_CASTLE_BLACK);

    score += longCastleBonus * (bool) (castlingFlag & LONG_CASTLE_WHITE);
    score -= longCastleBonus * (bool) (castlingFlag & LONG_CASTLE_BLACK);

    // King safety
    // Being in check is bad
    if (board.isInCheck(board.whiteTurn)) {
        if (board.whiteTurn) {
            score -= checkValue;
        } else {
            score += checkValue;
        }
    }

    const uint64_t whiteKingZone = kingAttacks[board.whiteKingSquare];
    const uint64_t blackKingZone = kingAttacks[board.blackKingSquare];

    // Better when protected with pawns
    int whiteKingPawnsCount = std::popcount(whiteKingZone & board.colorBB[WHITE][PAWN]);
    int blackKingPawnsCount = std::popcount(blackKingZone & board.colorBB[BLACK][PAWN]);

    score += kingPawnsBonus * (whiteKingPawnsCount - blackKingPawnsCount);

    /*SEEMS BAD... if (TEST_VAR) {
        int whiteAttackedKingZoneCount = std::popcount(whiteKingZone & capturesMaskColor[BLACK]);
        int blackAttackedKingZoneCount = std::popcount(blackKingZone & capturesMaskColor[WHITE]);

        score += attackedKingZoneMalus * (blackAttackedKingZoneCount - whiteAttackedKingZoneCount);
    }*/

    board.storeEvaluation(score);

    return score;
}


struct MoveResult {
    Move move;
    float score;
};

enum NodeType {
    NO_NODE,    // This entry is empty
    PV_NODE,    // Best moves, exact score
    CUT_NODE,   // Move which are "too good", lower bound score (relatively)
    ALL_NODE    // Bad moves, upper bound score (relatively)
};


// Transposition table
// TODO : Finish, Check efficiency, Check correctness, Use more
int halfMoveTick = 0;

struct TTEntry {
    uint64_t zobristHash = 0;
    int depth = 0;      // Higher depth is better because it means it appeared higher in the tree
    MoveResult result = {NO_MOVE, 0.0f};
    NodeType nodeType = NO_NODE;
    int creationTick = 0;    // Tick of creation
};

const uint64_t TTSize = 1ULL << TT_BITS;
const uint64_t TTMask = TTSize - 1ULL;
TTEntry transpositionTable[TTSize];

inline int getTTIndex(const Board &board) {
    return board.zobristHash & TTMask;
}
inline int relativeTTDepth(const TTEntry &entry) {
    return entry.depth - (halfMoveTick - entry.creationTick);
}
void updateTT(const Board &board, int depth, const MoveResult &result, NodeType nodeType) {
    TTEntry * currentEntry = &transpositionTable[getTTIndex(board)];

    /*if (currentEntry->nodeType != NO_NODE) {
        TTCollisionCount += 1;
    }*/

    if (currentEntry->nodeType == NO_NODE || relativeTTDepth(*currentEntry) < depth) {
        currentEntry->zobristHash = board.zobristHash;
        currentEntry->depth = depth;
        currentEntry->result = result;
        currentEntry->nodeType = nodeType;
        currentEntry->creationTick = halfMoveTick;
    }
}
// Use with PV nodes
void updateTT_PV(const Board &board, int depth, const MoveResult result) {
    transpositionTable[getTTIndex(board)] = {board.zobristHash, depth, result, PV_NODE, halfMoveTick};
}

std::vector<Move> principalVariation = {};

bool moveResultCompareIncreasing(MoveResult &a, MoveResult &b) {
    return a.score < b.score;
}
bool moveResultCompareDecreasing(MoveResult &a, MoveResult &b) {
    return a.score > b.score;
}


// TODO : Test efficiency and correctness   (depth = MAX_QUIESCENCE_DEPTH)
float quiescenceSearch(Board& board, int depth, float alpha, float beta) {    
    float baseScore = evaluatePosition(board);

    if (depth == 1) {
        return baseScore;
    }

    float bestScore = baseScore;
    
    bool whiteTurn = board.whiteTurn;

    if (whiteTurn) {
        if (baseScore >= beta) {
            return baseScore;
        }
        if (baseScore > alpha) {
            alpha = baseScore;
        }
    } else {
        if (baseScore <= alpha) {
            return baseScore;
        }
        if (baseScore < beta) {
            beta = baseScore;
        }
    }

    // Faster and better by only generating capture moves
    std::vector<Move> moves = {};

    board.getAllCaptureMoves(moves);

    const int moveCount = moves.size();

    if (moveCount == 0) {
        return baseScore;
    }

    // Move ordering + preparation
    MoveResult moveBaseEvaluations[moveCount] = {};
    // Board moveBoards[moves.size()] = {}; Find a way to sort the boards at the same time

    for (int i = 0 ; i < moveCount ; i++) {
        Move move = moves[i];

        // This value is not a normal evaluation !
        float value = piecesStandardValue[board.getAt(move.endSquare).type] - piecesStandardValue[board.getAt(move.startSquare).type];

        moveBaseEvaluations[i] = {move, value};
    }

    // The value used is not color dependant
    std::sort(moveBaseEvaluations, moveBaseEvaluations + moveCount, moveResultCompareDecreasing);

    for (int i = 0 ; i < moveCount ; i++) {
        MoveResult baseMoveResult = moveBaseEvaluations[i];

        nodeCount += 1;

        // Base score of the move
        float score = baseMoveResult.score;

        UnmakeMoveInfo info = board.playMove(baseMoveResult.move);
        score = quiescenceSearch(board, depth - 1, alpha, beta);
        board.undoMove(baseMoveResult.move, info);

        if (whiteTurn) {
            if (score >= beta) {
                return score;
            }
            if (score > bestScore) {
                bestScore = score;
            }
            alpha = score;
        } else {
            if (score <= alpha) {
                return score;
            }
            if (score < bestScore) {
                bestScore = score;
            }
            beta = score;
        }

        extensionCount += 1;
    }

    return bestScore;
}


/*Seems not effective... void makeSearchExtensions(Board &board, int &depth, int &remainingSearchExtensions) {
    if (depth == 1 && remainingSearchExtensions > 0) {
        // Extends if in check
        if (board.isInCheck(board.whiteTurn)) {
            depth += 1;
            remainingSearchExtensions -= 1;
            return;
        }
    }
}*/


// `depth >= 1` and should be odd for best performances ?
MoveResult search(Board &board, int depth=NORMAL_DEPTH, float alpha=-INFINITY, float beta=INFINITY/*, int remainingSearchExtensions=MAX_SEARCH_EXTENSION*/) {
    bool whiteTurn = board.whiteTurn;

    MoveResult bestMove;

   if (whiteTurn) {
        bestMove = {NO_MOVE, -INFINITY};
    } else {
        bestMove = {NO_MOVE, INFINITY};
    }

    // TODO : Issue here ?
    // Check if the position is present in the transposition table
    Move refutationMove = NO_MOVE;
    TTEntry entry = transpositionTable[getTTIndex(board)];
    if (entry.nodeType != NO_NODE && entry.zobristHash == board.zobristHash) {
        if (relativeTTDepth(entry) >= depth) {
            TTHitCount += 1;
            switch (entry.nodeType) {
            case PV_NODE:
                // The score is exact
                return entry.result;
            case CUT_NODE:
                // Score is lower bound (relatively)
                if (whiteTurn) {
                    if (entry.result.score >= beta) {
                        // Fail high
                        return entry.result;
                    }
                    // Not sure of correctness
                    if (entry.result.score > alpha) {
                        alpha = entry.result.score;
                    }
                } else {
                    if (entry.result.score <= alpha) {
                        // Fail high
                        return entry.result;
                    }
                    // Not sure of correctness
                    if (entry.result.score < beta) {
                        beta = entry.result.score;
                    }
                }
                break;
            case ALL_NODE:
                // Score is upper bound (relatively)
                if (whiteTurn) {
                    if (entry.result.score <= alpha) {
                        // Fail low
                        return entry.result;
                    }
                    // Not sure of correctness
                    if (entry.result.score < beta) {
                        beta = entry.result.score;
                    }
                } else {
                    if (entry.result.score >= beta) {
                        // Fail low
                        return entry.result;
                    }
                    // Not sure of correctness
                    if (entry.result.score > alpha) {
                        alpha = entry.result.score;
                    }
                }
                break;
            }
        }
        

        refutationMove = entry.result.move;
    }

    // TODO : Tweak parameters
    // Null Move Pruning, should always be before move generation
    if ( depth > NullMovePruningReduction && board.phase != ENDGAME && !board.isInCheck(whiteTurn) ) {
        board.playNullMove();
        MoveResult nullSearchResult;
        if (whiteTurn) {
            nullSearchResult = search(board, depth - NullMovePruningReduction, alpha, alpha + NULL_WINDOW_EPLISON);
        } else {
            nullSearchResult = search(board, depth - NullMovePruningReduction, beta - NULL_WINDOW_EPLISON, beta);
        }
        
        board.undoNullMove();

        if (whiteTurn) {
            if (nullSearchResult.score >= beta) {
                NMPCount += 1;
                // Do not update the TT as it is an illegal move
                return nullSearchResult;
            }
            // I don't know if it is correct
            // alpha = nullSearchResult.score;
        } else {
            if (nullSearchResult.score <= alpha) {
                NMPCount += 1;
                // Do not update the TT as it is an illegal move
                return nullSearchResult;
            }
            // beta = nullSearchResult.score;
        }
    }

    std::vector<Move> moves = {};
    board.getAllMoves(moves);

    const int moveCount = moves.size();

    if (moveCount == 0) {
        // This is a leaf node
        bestMove.score = evaluatePosition(board);
        updateTT(board, depth, bestMove, ALL_NODE);
        return bestMove;
    }

    if (depth > 1) {        
        // Move ordering
        MoveResult moveEvaluations[moveCount] = {};

        float refutationMoveBonus;
        float pvNodeBonus;
        float cutNodeBonus;

        if (whiteTurn) {
            pvNodeBonus = 40.0f;
            refutationMoveBonus = 20.0f;
            cutNodeBonus = 1.0f;
        } else {
            pvNodeBonus = -40.0f;
            refutationMoveBonus = -20.0f;
            cutNodeBonus = -1.0f;
        }

        for (int i = 0 ; i < moveCount ; i++) {
            float value;

            if (moves[i] == refutationMove) {                
                value = entry.result.score + refutationMoveBonus;
            } else {
                UnmakeMoveInfo info = board.playMove(moves[i]);

                TTEntry entry = transpositionTable[getTTIndex(board)];
                if (entry.nodeType != NO_NODE && entry.zobristHash == board.zobristHash) {
                    value = entry.result.score;

                    if (entry.nodeType == CUT_NODE) {
                        // Put cut nodes on top
                        value += cutNodeBonus;
                    } else if (entry.nodeType == PV_NODE) {
                        // Put pv nodes first
                        value += pvNodeBonus;
                        PVHitCount += 1;
                    }
                } else {
                    value = evaluatePosition(board);
                }

                board.undoMove(moves[i], info);
            }

            moveEvaluations[i] = {moves[i], value};
        }

        if (whiteTurn) {
            std::sort(moveEvaluations, moveEvaluations + moveCount, moveResultCompareDecreasing);
        } else {
            std::sort(moveEvaluations, moveEvaluations + moveCount, moveResultCompareIncreasing);
        }

        for (int i = 0 ; i < moveCount ; i++) {
            moves[i] = moveEvaluations[i].move;
        }
    }

    bool isLMRActive = false;       // Is enabled during search
    
    int moveIndex = 0;

    float score;
    MoveResult moveResult;

    for (const Move &move : moves) {
        UnmakeMoveInfo info = board.playMove(move);

        nodeCount += 1;

        if (depth == 1 || board.state != NEUTRAL) {
            // score = evaluatePosition(boardCopy);

            // Quiescence search
            //if ((whiteTurn && (score-initialBoardScore >= quiescenceThreshold)) || (!whiteTurn && (score-initialBoardScore <= -quiescenceThreshold))) {
            //if (board.isCapture(move)) {
                // score = moveResult.score;

            score = quiescenceSearch(board, MAX_QUIESCENCE_DEPTH, alpha, beta);
            
            extensionCount += 1;
        } else {
            if (depth > LMR_REDUCTION && depth <= maxLMRDepth && !isLMRActive) {
                // Triggers Late More Reduction
                if (moveCount >= LMR_MOVE_NUMBER) {
                    isLMRActive = true;
                }
            }

            if (isLMRActive) {
                moveResult = search(board, depth - LMR_REDUCTION, alpha, beta);

                // If better than excpected, full depth search is made
                if (whiteTurn) {
                    if (score >= alpha) {
                        moveResult = search(board, depth - 1, alpha, beta);
                    }
                } else {
                    if (score <= beta) {
                        moveResult = search(board, depth - 1, alpha, beta);
                    }
                }
            } else {
                moveResult = search(board, depth - 1, alpha, beta);
            }
            
            score = moveResult.score;
        }

        board.undoMove(move, info);

        if (whiteTurn) {
            if (score > bestMove.score) {
                bestMove = {move, score};

                if (score >= beta) {
                    // Fail high, cut node
                    updateTT(board, depth, bestMove, CUT_NODE);
                    return bestMove;
                }
                alpha = score;
            }
        } else {
            if (score < bestMove.score) {
                bestMove = {move, score};

                if (score <= alpha) {
                    // Fail high, Cut node
                    updateTT(board, depth, bestMove, CUT_NODE);
                    return bestMove;
                }
                beta = score;
            }
        }

        moveIndex += 1;
    }

    updateTT(board, depth, bestMove, ALL_NODE);
    
    return bestMove;
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

MoveResult getBestMove(Board &board, bool verbose=true, bool showBoard=false) {
    int depth = NORMAL_DEPTH;

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

    MoveResult bestResult;

    while (elapsedTime*5.0f < TARGET_BOT_TIME) {
        if (verbose) {
            printf("- Depth = %d\n", depth);
        }

        // Reset debug variables
        nodeCount = 1;
        evaluationCounter = 0;
        extensionCount = 0;
        NMPCount = 0;
        PVHitCount = 0;
        TTCollisionCount = 0;
        TTHitCount = 0;
        
        bestResult = search(board, depth);

        // Stores all PV nodes
        principalVariation.clear();

        Board pvBoard = board.copy();
        int pvDepth = depth;
        float pvScore = bestResult.score;

        Move pvMove = bestResult.move;
        while (pvDepth > 1) {
            updateTT_PV(pvBoard, pvDepth, {pvMove, pvScore});
            principalVariation.push_back(pvMove);

            pvDepth -= 1;
            pvBoard.playMove(pvMove);
            if (showBoard) {
                printBoard(pvBoard);
                printf("  ----\n");
            }

            TTEntry * entry = &transpositionTable[getTTIndex(pvBoard)];
            if (entry->nodeType == ALL_NODE && entry->zobristHash == pvBoard.zobristHash && entry->result.score == pvScore) {
                pvMove = entry->result.move;
            } else {
                pvMove = entry->result.move;
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

        depth += 1;
    }

    board.cleanPreviousHashes();

    if (verbose) {
        printf("| Nodes : %d\n", nodeCount);
        printf("| Evaluations : %d\n", evaluationCounter);
        printf("| Extensions : %d\n", extensionCount);
        printf("| NMP : %d\n", NMPCount);
        printf("| PV hits : %d\n", PVHitCount);
        printf("| TT Hit : %d\n", TTHitCount);
        // printf("| TT Hit : %d, Collisions : %d\n", TTHitCount, TTCollisionCount);

        printf("| Bot took %f milliseconds\n", elapsedTime);
    }

    return bestResult;
}
