#ifndef BOT_CONSTANTS
#define BOT_CONSTANTS

#include "gameConstants.cpp"


// Less mesures = better performances
#define NO_MESURE 0
#define LOW_MESURE 1
#define ALL_MESURE 2
#define MESURE_LEVEL ALL_MESURE // ALL_MESURE


//    Time controls
const bool IS_GAME_TIMED = false;        // NOT IMPLEMENTED
// if IS_GAME_TIMED is true, target bot is ignored. Else, it is the only used
const float TOTAL_GAME_TIME = 60000.0f;        // NOT IMPLEMENTED
const float TIME_INCREMENT = 5000.0f;        // NOT IMPLEMENTED

// Time that the bot can take, in milliseconds
const float DEFAULT_BOT_TIME = /*100.0f;*/ 5000.0f /*0.0001f*/;
// Time at which the search is cancelled it is too long, in milliseconds
const float MAX_BOT_TIME = 20000.0f;    // 20000.0f;

//    General settings

const int TT_BITS = 22; // 20;
const uint64_t TTSize = 1ULL << TT_BITS;
const uint64_t TTMask = TTSize - 1ULL;

const int NORMAL_DEPTH = 8; // 8 (minimal depth searched)
const int MAX_QUIESCENCE_DEPTH = 4; //4;   // Limits Quiescence Search - 
const int MAX_SEARCH_EXTENSION = 1;

//    Evaluation values
// Values are in centipawns

// Should have some margin from INT32_MIN to prevent underflows
const int INFINITE_SCORE =       999999'99;
const int CHECKMATE_BASE_SCORE = 9999'99;

const int checkValue = 60;
const int mobilityValue = 5;

// Between 0 and 128, proportion of points keeped for being attacked (feels danger)
const int attackedPenaltyRatio = 0.70 * 128;
// Proportion of points keeped for being attacked and defended at the same time
const int attackedDefendedPenaltyRatio = 0.85 * 128;

// Given when a player has the right to play
const int turnBonus = 20;
// Bonus when the player has at least two bishops
const int bishopPairBonus = 40;
// Penalty for having two knights (redundency)
const int knightPairPenalty = 20;
// Penalty for having two rooks (redundency)
const int rookPairPenalty = 20;

// Bonus for having short castle available
const int shortCastleBonus = 30;
// Bonus for having long castle available
const int longCastleBonus = 20;

// Bonus for having rook aligned with the opponent's queen
const int rookQueenAlignedBonus = 15;
// Bonus for having rook in a column with only pawns
const int rookSemiOpenColumnBonus = 20;
// Bonus for having rook in a column with no other piece
const int rookOpenColumnBonus = 30;
// Bonus for being defended by another rook
const int rookConnectedBonus = 15;

// Bonus for each pawn protecting a pawn/knight
const int pawnProtectsBonus = 10;
// Malus for each isolated pawn (with no pawn of the same color in an adjascent column)
const int isolatedPawnMalus = 30;
// Malus for each overextended pawn (with no pawn in adjascent columns, which are at most 2 squares behind)
// can't cumulate with isolated pawn malus
const int overextendedPawnMalus = 15;
// Bonus for having a pawn with no enemy pawns in the way (takes the y position for black and 7-y for white)
const int passedPawnBonuses[8] = {
    00,   // impossible
    30,
    40,
    55,
    70,
    90,
    100,
    00    // impossible
};

// Bonus for each pawn near the king
const int kingPawnsBonus = 20;
// Malus for each square of the king zone which is attacked
const int attackedKingZoneMalus = 15;
// Malus for each one open file next to the king
const int kingOpenFilesMalus = 30;
// Malus for each square of virtual mobility for the king when replaced by a queen
const int kingVirtualMobilityMalus = 5;

const int FUTILITY_MARGIN = 300;

// DEPRECATED, NOW BASED ON CAPTURES - Lower = less strict = more search
// const float quiescenceThreshold = 0.65f;*/

// Reduction of depth during a null move pruning, includes normal depth decrement
const int NullMovePruningReduction = 3;
const int NMPRejectMargin = 50;      // If the position is farther from the pruning bounds, avoid NMP

// Starts late Move Reduction when depth is smaller than this value
const int maxLMRDepth = NORMAL_DEPTH - 2;
const int LMR_MOVE_NUMBER = 4;  // Number of moves from which LMR is applied

const int DELTA_PRUNING_MARGIN = 200;

// Pawn structure - Penalty of having too much pawns aligned
const int alignedPawnPenalties[8] = {
    000,
    035,
    075,
    100,      // Almost impossible
    130,
    160,
    190,
    220,
};

const int piecesStandardValue[6] = {
    100,
    300,
    300,
    500,
    900,
    000
};

// From white's perspective
int pieceValuesPosOpening[6][8][8] = {
    {     // PAWN
        {200, 200, 200, 200, 200, 200, 200, 200},
        {170, 170, 170, 170, 170, 170, 170, 170},
        {135, 135, 140, 165, 165, 140, 135, 135},
        {115, 120, 130, 155, 155, 130, 120, 115},
        {108, 110, 115, 150, 150, 115, 110, 108},
        {105, 105, 105, 110, 110, 105, 105, 105},
        {115, 115, 100,  95,  95, 100, 115, 115},
        {000, 000, 000, 000, 000, 000, 000, 000}
    }, {  // KNIGHT
        {270, 280, 290, 300, 300, 290, 280, 270},
        {280, 305, 305, 305, 305, 305, 305, 280},
        {285, 310, 320, 325, 325, 320, 310, 285},
        {285, 320, 330, 330, 330, 330, 320, 285},
        {285, 320, 330, 325, 325, 330, 320, 285},
        {285, 300, 320, 310, 310, 320, 300, 285},
        {280, 295, 300, 300, 300, 300, 295, 280},
        {270, 280, 290, 300, 300, 290, 280, 270}
    }, {  // BISHOP
        {280, 290, 290, 290, 290, 290, 290, 280},
        {290, 300, 300, 300, 300, 300, 300, 290},
        {290, 300, 300, 300, 300, 300, 300, 290},
        {290, 310, 305, 315, 315, 305, 310, 290},
        {290, 310, 310, 315, 315, 310, 310, 290},
        {290, 310, 305, 310, 310, 305, 310, 290},
        {290, 315, 300, 300, 300, 300, 315, 290},
        {280, 290, 290, 290, 290, 290, 290, 280}
    }, {  // ROOK
        {490, 500, 500, 500, 500, 500, 500, 490},
        {495, 505, 505, 505, 505, 505, 505, 495},
        {495, 500, 500, 500, 500, 500, 500, 495},
        {495, 500, 500, 500, 500, 500, 500, 495},
        {495, 500, 500, 500, 500, 500, 500, 495},
        {495, 500, 500, 500, 500, 500, 500, 495},
        {500, 510, 510, 510, 510, 510, 510, 500},
        {510, 505, 515, 520, 520, 515, 505, 510}
    }, {  // QUEEN
        {870, 900, 900, 900, 900, 900, 900, 870},
        {880, 890, 890, 890, 890, 890, 890, 880},
        {860, 860, 870, 875, 875, 870, 860, 860},
        {840, 845, 855, 855, 855, 855, 845, 840},
        {840, 850, 855, 855, 855, 855, 850, 840},
        {850, 860, 860, 860, 860, 860, 860, 850},
        {860, 890, 890, 890, 890, 890, 890, 860},
        {860, 880, 900, 900, 900, 900, 880, 860}
    }, {  // KING
        {-70, -70, -70, -70, -70, -70, -70, -70},
        {-60, -60, -60, -60, -60, -60, -60, -60},
        {-50, -50, -50, -50, -50, -50, -50, -50},
        {-40, -40, -40, -40, -40, -40, -40, -40},
        {-30, -30, -30, -30, -30, -30, -30, -30},
        {-20, -20, -20, -20, -20, -20, -20, -20},
        {-10, -10, -10, -10, -10, -10, -10, -10},
        { 40,  70,  40,  15,  15,  40,  70,  40}
    }
};

/*
NOT USED ANYMORE
int pieceValuesPosMiddlegame[6][8][8] = {
    {     // PAWN
        {200, 200, 200, 200, 200, 200, 200, 200},
        {160, 160, 160, 165, 165, 160, 160, 160},
        {140, 140, 140, 150, 150, 140, 140, 140},
        {115, 120, 125, 145, 145, 125, 120, 115},
        {110, 110, 120, 140, 140, 120, 110, 110},
        {108, 105, 105, 110, 110, 105, 105, 108},
        {115, 115, 110, 100, 100, 110, 115, 115},
        {000, 000, 000, 000, 000, 000, 000, 000}
    }, {  // KNIGHT
        {270, 280, 290, 300, 300, 290, 280, 270},
        {280, 305, 305, 305, 305, 305, 305, 280},
        {285, 310, 320, 325, 325, 320, 310, 285},
        {285, 320, 330, 330, 330, 330, 320, 285},
        {285, 320, 330, 325, 325, 330, 320, 285},
        {285, 300, 320, 310, 310, 320, 300, 285},
        {280, 295, 300, 300, 300, 300, 295, 280},
        {270, 280, 290, 300, 300, 290, 280, 270}
    }, {  // BISHOP
        {280, 290, 290, 290, 290, 290, 290, 280},
        {290, 300, 300, 300, 300, 300, 300, 290},
        {290, 300, 300, 300, 300, 300, 300, 290},
        {290, 310, 305, 315, 315, 305, 310, 290},
        {290, 310, 310, 315, 315, 310, 310, 290},
        {290, 310, 305, 310, 310, 305, 310, 290},
        {290, 315, 300, 300, 300, 300, 315, 290},
        {280, 290, 290, 290, 290, 290, 290, 280}
    }, {  // ROOK
        {500, 505, 510, 510, 510, 510, 505, 500},
        {505, 510, 515, 520, 520, 515, 510, 505},
        {500, 505, 510, 510, 510, 510, 505, 500},
        {495, 500, 500, 505, 505, 500, 500, 495},
        {495, 500, 500, 500, 500, 500, 500, 495},
        {495, 500, 505, 510, 510, 505, 500, 495},
        {500, 510, 510, 515, 515, 510, 510, 500},
        {510, 505, 515, 520, 520, 515, 505, 510}
    }, {  // QUEEN
        {890, 900, 900, 900, 900, 900, 900, 890},
        {895, 910, 910, 905, 905, 910, 910, 895},
        {895, 910, 910, 908, 908, 910, 910, 895},
        {895, 910, 910, 910, 910, 910, 910, 895},
        {895, 910, 910, 910, 910, 910, 910, 895},
        {895, 910, 910, 908, 908, 910, 910, 895},
        {895, 910, 910, 905, 905, 910, 910, 895},
        {890, 895, 900, 900, 900, 900, 895, 890}
    }, {  // KING
        {-40, -40, -40, -40, -40, -40, -40, -40},
        {-35, -35, -35, -35, -35, -35, -35, -35},
        {-30, -30, -30, -30, -30, -30, -30, -30},
        {-25, -25, -25, -25, -25, -25, -25, -25},
        {-20, -20, -20, -20, -20, -20, -20, -20},
        {-15, -15, -15, -15, -15, -15, -15, -15},
        { 00,  00,  00,  00,  00,  00,  00,  00},
        { 40,  70,  40,  10,  10,  40,  70,  40}
    }
};*/

int pieceValuesPosEndgame[6][8][8] = {
    {     // PAWN
        {200, 200, 200, 200, 200, 200, 200, 200},
        {190, 190, 190, 190, 190, 190, 190, 190},
        {160, 160, 160, 160, 160, 160, 160, 160},
        {145, 145, 145, 150, 150, 145, 145, 145},
        {130, 130, 130, 140, 140, 130, 130, 130},
        {120, 120, 120, 120, 120, 120, 120, 120},
        {100, 100, 100, 100, 100, 100, 100, 100},
        {000, 000, 000, 000, 000, 000, 000, 000}
    }, {  // KNIGHT
        {260, 270, 280, 290, 290, 280, 270, 260},
        {270, 295, 295, 295, 295, 295, 295, 270},
        {275, 300, 310, 315, 315, 310, 300, 275},
        {275, 310, 325, 330, 330, 325, 310, 275},
        {275, 310, 320, 315, 315, 320, 310, 275},
        {275, 290, 310, 300, 300, 310, 290, 275},
        {270, 285, 290, 290, 290, 290, 285, 270},
        {260, 270, 280, 290, 290, 280, 270, 260}
    }, {  // BISHOP
        {280, 290, 290, 290, 290, 290, 290, 280},
        {290, 300, 300, 300, 300, 300, 300, 290},
        {290, 300, 300, 300, 300, 300, 300, 290},
        {290, 310, 305, 315, 315, 305, 310, 290},
        {290, 310, 310, 315, 315, 310, 310, 290},
        {290, 310, 305, 310, 310, 305, 310, 290},
        {290, 315, 300, 300, 300, 300, 315, 290},
        {280, 290, 290, 290, 290, 290, 290, 280}
    }, {  // ROOK
        {490, 500, 505, 505, 505, 505, 500, 490},
        {495, 515, 525, 525, 525, 525, 515, 495},
        {500, 510, 520, 520, 520, 520, 510, 500},
        {500, 505, 512, 515, 515, 512, 505, 500},
        {495, 500, 507, 505, 505, 507, 500, 495},
        {495, 500, 500, 500, 500, 500, 500, 495},
        {495, 510, 510, 510, 510, 510, 510, 495},
        {490, 510, 515, 518, 518, 515, 510, 490}
    }, {  // QUEEN
        {900, 905, 905, 905, 905, 905, 905, 900},
        {905, 930, 930, 930, 930, 930, 930, 905},
        {905, 930, 935, 935, 935, 935, 930, 905},
        {905, 930, 935, 938, 938, 935, 930, 905},
        {905, 930, 935, 938, 938, 935, 930, 905},
        {905, 930, 935, 935, 935, 935, 930, 905},
        {905, 930, 930, 930, 930, 930, 930, 905},
        {900, 905, 905, 905, 905, 905, 905, 900}
    }, {  // KING
        {-10, -05, 000, 000, 000, 000, -05, -10},
        {-05, 015, 030, 035, 035, 030, 015, -05},
        {000, 030, 045, 050, 050, 045, 030, 000},
        {000, 035, 050, 055, 055, 050, 035, 000},
        {000, 035, 050, 055, 055, 050, 035, 000},
        {000, 030, 045, 050, 050, 045, 030, 000},
        {-05, 015, 030, 035, 035, 030, 015, -05},
        {-10, -05, 000, 000, 000, 000, -05, -10}
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
int pieceValuesPosOpeningColor[2][PIECE_TYPE_COUNT][64];
// int pieceValuesPosMiddlegameColor[2][PIECE_TYPE_COUNT][64];
int pieceValuesPosEndgameColor[2][PIECE_TYPE_COUNT][64];

constexpr void initBot() {
    for (int typeIndex = 0 ; typeIndex < PIECE_TYPE_COUNT ; typeIndex++) {
        for (int y = 0 ; y < 8 ; y++) {
            for (int x = 0 ; x < 8 ; x++) {
                int square = x + 8u*y;

                // Copies the values
                pieceValuesPosOpeningColor[WHITE][typeIndex][square] = pieceValuesPosOpening[typeIndex][y][x];
                // pieceValuesPosMiddlegameColor[WHITE][typeIndex][square] = pieceValuesPosMiddlegame[typeIndex][y][x];
                pieceValuesPosEndgameColor[WHITE][typeIndex][square] = pieceValuesPosEndgame[typeIndex][y][x];

                // Same but negative and reversed
                pieceValuesPosOpeningColor[BLACK][typeIndex][square] = -pieceValuesPosOpening[typeIndex][7 - y][x];
                // pieceValuesPosMiddlegameColor[BLACK][typeIndex][square] = -pieceValuesPosMiddlegame[typeIndex][7 - y][x];
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

// Gives the base value of pieces using piece square tables 
inline int piecesValue(PieceType pieceType, bool isWhite, uint64_t occupency, int (*pieceValuesPosColor)[PIECE_TYPE_COUNT][64]) {
    int sum = 0;
    while (occupency) {
        sum += pieceValuesPosColor[isWhite][pieceType][popLastSquare(occupency)];
    }
    return sum;
}

#endif
