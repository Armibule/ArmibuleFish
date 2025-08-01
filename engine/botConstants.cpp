#include "gameConstants.cpp"

// Variable used to test features
bool TEST_VAR;

// Time that the bot can take if he wants more depth, in milliseconds
const float TARGET_BOT_TIME = /*100.0f;*/ 3000.0f; // 0.0001f;

const int TT_BITS = 20; // 20;

const int NORMAL_DEPTH = 8; // 7;
const int MAX_QUIESCENCE_DEPTH = 4;   // Limits Quiescence Search
// const int MAX_SEARCH_EXTENSION = 1;

const float checkValue = 0.5f;
const float mobilityValue = 0.05f;

// Between 0 and 1, proportion of points keeped for being attacked (feels danger)
const float attackedPenaltyRatio = 0.65f;
// Proportion of points keeped for being attacked and defended at the same time
const float attackedDefendedPenaltyRatio = 0.80f;

// Given when a player has the right to play
const float turnBonus = 0.2f;
// Bonus when the player has at least two bishops
const float bishopPairBonus = 0.4f;
// Penalty for having two knights (redundency)
const float knightPairPenalty = 0.2f;
// Penalty for having two rooks (redundency)
const float rookPairPenalty = 0.2f;

// Bonus for having short castle available
const float shortCastleBonus = 0.3f;
// Bonus for having long castle available
const float longCastleBonus = 0.2f;

// Bonus for having rook aligned with the opponent's queen
const float rookQueenAlignedBonus = 0.15f;
// Bonus for having rook in a column with only pawns
const float rookSemiOpenColumnBonus = 0.2f;
// Bonus for having rook in a column with no other piece
const float rookOpenColumnBonus = 0.3f;

// Bonus for each pawn protecting a pawn/knight
const float pawnProtectsBonus = 0.1f;
// Malus for each isolated pawn (with no pawn of the same color in an adjascent column)
const float isolatedPawnMalus = 0.2f;
// Bonus for having a pawn with no enemy pawns in the way (takes the y position for black and 7-y for white)
const float passedPawnBonuses[8] = {
    0.30f,   // impossible
    0.30f,
    0.35f,
    0.40f,
    0.45f,
    0.55f,
    0.70f,
    0.00f    // impossible
};

// Bonus for each pawn near the king
const float kingPawnsBonus = 0.20f;
// Malus for each square of the king zone which is attacked
const float attackedKingZoneMalus = 0.15f;

// DEPRECATED, NOW BASED ON CAPTURES - Lower = less strict = more search
// const float quiescenceThreshold = 0.65f;

/*BAD float quiescenceThresholdMinGain = 0.5f;
float quiescenceThresholdMaxLoss = -2.8f;*/

// Reduction of depth during a null move pruning, includes normal depth decrement
const int NullMovePruningReduction = 3;

// Starts late Move Reduction when depth is smaller than this value
const int maxLMRDepth = NORMAL_DEPTH - 2;
// DERECATED const int LMRLossThreshold = -0.9f;     // More negative = more strict
const int LMR_MOVE_NUMBER = 3;  // Number of moves after which LMR is applied
const int LMR_REDUCTION = 2;    // Includes normal depth decrement (1 => nothing happens)

const float NULL_WINDOW_EPLISON = 0.001f;

// Pawn structure - Penalty of having too much pawns aligned
const float alignedPawnPenalties[8] = {
    0.00f,
    0.35f,
    0.75f,
    1.00f,      // Almost impossible
    1.30f,
    1.60f,
    1.90f,
    2.20f,
};

const float piecesStandardValue[6] = {
    1.0f,
    3.0f,
    3.0f,
    5.0f,
    9.0f,
    0.0f
};

// From white's perspective
float pieceValuesPosOpening[6][8][8] = {
    {     // PAWN
        {2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00},
        {1.60, 1.60, 1.60, 1.70, 1.70, 1.60, 1.60, 1.60},
        {1.30, 1.30, 1.35, 1.60, 1.60, 1.35, 1.30, 1.30},
        {1.15, 1.15, 1.20, 1.55, 1.55, 1.20, 1.15, 1.15},
        {1.10, 1.10, 1.15, 1.50, 1.50, 1.15, 1.10, 1.10},
        {1.08, 1.05, 1.05, 1.10, 1.10, 1.05, 1.05, 1.08},
        {1.10, 1.10, 1.00, 1.00, 1.00, 1.00, 1.10, 1.10},
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00}
    }, {  // KNIGHT
        {2.70, 2.80, 2.90, 3.00, 3.00, 2.90, 2.80, 2.70},
        {2.80, 3.05, 3.05, 3.05, 3.05, 3.05, 3.05, 2.80},
        {2.85, 3.10, 3.20, 3.20, 3.20, 3.20, 3.10, 2.85},
        {2.85, 3.20, 3.30, 3.30, 3.30, 3.30, 3.20, 2.85},
        {2.85, 3.20, 3.30, 3.25, 3.25, 3.30, 3.20, 2.85},
        {2.85, 3.00, 3.20, 3.10, 3.10, 3.20, 3.00, 2.85},
        {2.80, 2.95, 3.00, 3.00, 3.00, 3.00, 2.95, 2.80},
        {2.70, 2.80, 2.90, 3.00, 3.00, 2.90, 2.80, 2.70}
    }, {  // BISHOP
        {2.80, 2.90, 2.90, 2.90, 2.90, 2.90, 2.90, 2.80},
        {2.90, 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 2.90},
        {2.90, 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 2.90},
        {2.90, 3.10, 3.05, 3.15, 3.15, 3.05, 3.10, 2.90},
        {2.90, 3.10, 3.10, 3.15, 3.15, 3.10, 3.10, 2.90},
        {2.90, 3.10, 3.05, 3.10, 3.10, 3.05, 3.10, 2.90},
        {2.90, 3.15, 3.00, 3.00, 3.00, 3.00, 3.15, 2.90},
        {2.80, 2.90, 2.90, 2.90, 2.90, 2.90, 2.90, 2.80}
    }, {  // ROOK
        {5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00},
        {4.95, 5.05, 5.05, 5.05, 5.05, 5.05, 5.05, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.10, 5.10, 5.10, 5.10, 5.10, 5.10, 4.95},
        {5.00, 5.10, 5.15, 5.20, 5.20, 5.15, 5.10, 5.00}
    }, {  // QUEEN
        {8.70, 9.00, 9.00, 9.00, 9.00, 9.00, 9.00, 8.70},
        {8.80, 8.90, 8.90, 8.90, 8.90, 8.90, 8.90, 8.80},
        {8.60, 8.70, 8.70, 8.75, 8.75, 8.70, 8.70, 8.60},
        {8.50, 8.55, 8.55, 8.55, 8.55, 8.55, 8.55, 8.50},
        {8.50, 8.60, 8.60, 8.60, 8.60, 8.60, 8.60, 8.50},
        {8.60, 8.70, 8.60, 8.60, 8.60, 8.60, 8.70, 8.60},
        {8.70, 8.90, 8.90, 8.90, 8.90, 8.90, 8.90, 8.70},
        {8.60, 8.80, 9.00, 9.00, 9.00, 9.00, 8.80, 8.60}
    }, {  // KING
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {0.40, 0.70, 0.40, 0.15, 0.15, 0.40, 0.70, 0.40}
    }
};

float pieceValuesPosMiddlegame[6][8][8] = {
    {     // PAWN
        {2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00},
        {1.60, 1.60, 1.60, 1.65, 1.65, 1.60, 1.60, 1.60},
        {1.40, 1.40, 1.40, 1.50, 1.50, 1.40, 1.40, 1.40},
        {1.20, 1.20, 1.25, 1.45, 1.45, 1.25, 1.20, 1.20},
        {1.10, 1.10, 1.20, 1.40, 1.40, 1.20, 1.10, 1.10},
        {1.08, 1.05, 1.05, 1.10, 1.10, 1.05, 1.05, 1.08},
        {1.15, 1.15, 1.10, 1.00, 1.00, 1.10, 1.15, 1.15},
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00}
    }, {  // KNIGHT
        {2.70, 2.80, 2.90, 3.00, 3.00, 2.90, 2.80, 2.70},
        {2.80, 3.05, 3.05, 3.05, 3.05, 3.05, 3.05, 2.80},
        {2.85, 3.10, 3.20, 3.20, 3.20, 3.20, 3.10, 2.85},
        {2.85, 3.20, 3.30, 3.30, 3.30, 3.30, 3.20, 2.85},
        {2.85, 3.20, 3.30, 3.25, 3.25, 3.30, 3.20, 2.85},
        {2.85, 3.00, 3.20, 3.10, 3.10, 3.20, 3.00, 2.85},
        {2.80, 2.95, 3.00, 3.00, 3.00, 3.00, 2.95, 2.80},
        {2.70, 2.80, 2.90, 3.00, 3.00, 2.90, 2.80, 2.70}
    }, {  // BISHOP
        {2.80, 2.90, 2.90, 2.90, 2.90, 2.90, 2.90, 2.80},
        {2.90, 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 2.90},
        {2.90, 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 2.90},
        {2.90, 3.10, 3.05, 3.15, 3.15, 3.05, 3.10, 2.90},
        {2.90, 3.10, 3.10, 3.15, 3.15, 3.10, 3.10, 2.90},
        {2.90, 3.10, 3.05, 3.10, 3.10, 3.05, 3.10, 2.90},
        {2.90, 3.15, 3.00, 3.00, 3.00, 3.00, 3.15, 2.90},
        {2.80, 2.90, 2.90, 2.90, 2.90, 2.90, 2.90, 2.80}
    }, {  // ROOK
        {5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00},
        {4.95, 5.05, 5.05, 5.05, 5.05, 5.05, 5.05, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.05, 5.05, 5.05, 5.05, 5.00, 4.95},
        {5.00, 5.10, 5.10, 5.15, 5.15, 5.10, 5.10, 5.00},
        {5.00, 5.10, 5.15, 5.20, 5.20, 5.15, 5.10, 5.00}
    }, {  // QUEEN
        {8.90, 9.00, 9.00, 9.00, 9.00, 9.00, 9.00, 8.90},
        {8.95, 9.10, 9.10, 9.05, 9.05, 9.10, 9.10, 8.95},
        {8.95, 9.10, 9.10, 9.08, 9.08, 9.10, 9.10, 8.95},
        {8.95, 9.10, 9.10, 9.10, 9.10, 9.10, 9.10, 8.95},
        {8.95, 9.10, 9.10, 9.10, 9.10, 9.10, 9.10, 8.95},
        {8.95, 9.10, 9.10, 9.08, 9.08, 9.10, 9.10, 8.95},
        {8.95, 9.10, 9.10, 9.05, 9.05, 9.10, 9.10, 8.95},
        {8.90, 8.95, 9.00, 9.00, 9.00, 9.00, 8.95, 8.90}
    }, {  // KING
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {-0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1, -0.1},
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00},
        {0.40, 0.70, 0.40, 0.10, 0.10, 0.40, 0.70, 0.40}
    }
};

float pieceValuesPosEndgame[6][8][8] = {
    {     // PAWN
        {2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00, 2.00},
        {1.80, 1.80, 1.80, 1.80, 1.80, 1.80, 1.80, 1.80},
        {1.50, 1.50, 1.50, 1.50, 1.50, 1.50, 1.50, 1.50},
        {1.40, 1.40, 1.40, 1.45, 1.45, 1.40, 1.40, 1.40},
        {1.30, 1.30, 1.30, 1.40, 1.40, 1.30, 1.30, 1.30},
        {1.20, 1.20, 1.20, 1.20, 1.20, 1.20, 1.20, 1.20},
        {1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00},
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00}
    }, {  // KNIGHT
        {2.70, 2.80, 2.90, 3.00, 3.00, 2.90, 2.80, 2.70},
        {2.80, 3.05, 3.05, 3.05, 3.05, 3.05, 3.05, 2.80},
        {2.85, 3.10, 3.20, 3.20, 3.20, 3.20, 3.10, 2.85},
        {2.85, 3.20, 3.30, 3.30, 3.30, 3.30, 3.20, 2.85},
        {2.85, 3.20, 3.30, 3.25, 3.25, 3.30, 3.20, 2.85},
        {2.85, 3.00, 3.20, 3.10, 3.10, 3.20, 3.00, 2.85},
        {2.80, 2.95, 3.00, 3.00, 3.00, 3.00, 2.95, 2.80},
        {2.70, 2.80, 2.90, 3.00, 3.00, 2.90, 2.80, 2.70}
    }, {  // BISHOP
        {2.80, 2.90, 2.90, 2.90, 2.90, 2.90, 2.90, 2.80},
        {2.90, 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 2.90},
        {2.90, 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 2.90},
        {2.90, 3.10, 3.05, 3.15, 3.15, 3.05, 3.10, 2.90},
        {2.90, 3.10, 3.10, 3.15, 3.15, 3.10, 3.10, 2.90},
        {2.90, 3.10, 3.05, 3.10, 3.10, 3.05, 3.10, 2.90},
        {2.90, 3.15, 3.00, 3.00, 3.00, 3.00, 3.15, 2.90},
        {2.80, 2.90, 2.90, 2.90, 2.90, 2.90, 2.90, 2.80}
    }, {  // ROOK
        {5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00},
        {4.95, 5.15, 5.15, 5.15, 5.15, 5.15, 5.15, 4.95},
        {4.95, 5.10, 5.10, 5.10, 5.10, 5.10, 5.10, 4.95},
        {4.95, 5.05, 5.05, 5.05, 5.05, 5.05, 5.05, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.00, 5.00, 5.00, 5.00, 5.00, 5.00, 4.95},
        {4.95, 5.10, 5.10, 5.10, 5.10, 5.10, 5.10, 4.95},
        {5.00, 5.10, 5.15, 5.18, 5.18, 5.15, 5.10, 5.00}
    }, {  // QUEEN
        {8.90, 8.95, 8.95, 8.95, 8.95, 8.95, 8.95, 8.90},
        {8.95, 9.20, 9.20, 9.20, 9.20, 9.20, 9.20, 8.95},
        {8.95, 9.20, 9.25, 9.25, 9.25, 9.25, 9.20, 8.95},
        {8.95, 9.20, 9.25, 9.28, 9.28, 9.25, 9.20, 8.95},
        {8.95, 9.20, 9.25, 9.28, 9.28, 9.25, 9.20, 8.95},
        {8.95, 9.20, 9.25, 9.25, 9.25, 9.25, 9.20, 8.95},
        {8.95, 9.20, 9.20, 9.20, 9.20, 9.20, 9.20, 8.95},
        {8.90, 8.95, 8.95, 8.95, 8.95, 8.95, 8.95, 8.90}
    }, {  // KING
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00},
        {0.00, 0.05, 0.20, 0.20, 0.20, 0.20, 0.05, 0.00},
        {0.00, 0.20, 0.25, 0.30, 0.30, 0.25, 0.20, 0.00},
        {0.00, 0.20, 0.30, 0.35, 0.35, 0.30, 0.20, 0.00},
        {0.00, 0.20, 0.30, 0.35, 0.35, 0.30, 0.20, 0.00},
        {0.00, 0.20, 0.25, 0.30, 0.30, 0.25, 0.20, 0.00},
        {0.00, 0.05, 0.20, 0.20, 0.20, 0.20, 0.05, 0.00},
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00}
    }
};

// Mask containing the two adjacent columns of the pawn (one if on the side)
const uint64_t isolatedPawnMasks[8] = {
    columnMasks[1],
    columnMasks[0] | columnMasks[2],
    columnMasks[1] | columnMasks[3],
    columnMasks[2] | columnMasks[4],
    columnMasks[3] | columnMasks[5],
    columnMasks[4] | columnMasks[6],
    columnMasks[5] | columnMasks[7],
    columnMasks[6]
};
// Mask containing the three columns in front of the pawn, initialized with initBot()
uint64_t passedPawnMasksWhite[8][8];
uint64_t passedPawnMasksBlack[8][8];

// From BLACK, WHITE perspective
float pieceValuesPosOpeningColor[2][PIECE_TYPE_COUNT][64];
float pieceValuesPosMiddlegameColor[2][PIECE_TYPE_COUNT][64];
float pieceValuesPosEndgameColor[2][PIECE_TYPE_COUNT][64];

constexpr void initBot() {
    for (int typeIndex = 0 ; typeIndex < PIECE_TYPE_COUNT ; typeIndex++) {
        for (int y = 0 ; y < 8 ; y++) {
            for (int x = 0 ; x < 8 ; x++) {
                int square = x + 8u*y;

                // Copies the values
                pieceValuesPosOpeningColor[WHITE][typeIndex][square] = pieceValuesPosOpening[typeIndex][y][x];
                pieceValuesPosMiddlegameColor[WHITE][typeIndex][square] = pieceValuesPosMiddlegame[typeIndex][y][x];
                pieceValuesPosEndgameColor[WHITE][typeIndex][square] = pieceValuesPosEndgame[typeIndex][y][x];

                // Same but negative and reversed
                pieceValuesPosOpeningColor[BLACK][typeIndex][square] = -pieceValuesPosOpening[typeIndex][7 - y][x];
                pieceValuesPosMiddlegameColor[BLACK][typeIndex][square] = -pieceValuesPosMiddlegame[typeIndex][7 - y][x];
                pieceValuesPosEndgameColor[BLACK][typeIndex][square] = -pieceValuesPosEndgame[typeIndex][7 - y][x];
            }
        }
    }

    for (int y = 0 ; y < 8 ; y++) {
        for (int x = 0 ; x < 8 ; x++) {
            uint64_t columns = columnMasks[x];
            if (x > 0) {
                columns |= columnMasks[x-1];
            }
            if (x < 7) {
                columns |= columnMasks[x+1];
            }

            passedPawnMasksWhite[y][x] = columns << (8 * (8 - y));
            passedPawnMasksBlack[y][x] = columns >> (8 * (y + 1));
        }
    }
}

