#ifndef GAME_CONSTANTS
#define GAME_CONSTANTS


#include <cstdint>
#include <vector>
#include "gameConstantsGenerator.cpp"
#include <string>


enum PieceType : char {
    EMPTY = -1,     // Marks an empty square
    PAWN = 0,
    BISHOP = 1,
    KNIGHT = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5
};

enum GameState : int {
    NEUTRAL,
    WHITE_WON,
    BLACK_WON,
    DRAW
};
// Number between 0 and 256, represents the progress from opening to endgame
/*typedef int GamePhase;
const GamePhase INITIAL_GAME_PHASE = 0;
const GamePhase ENDGAME_THRESHOLD = 180;*/

enum MoveType {
    NORMAL_MOVE,
    SHORT_CASTLE,     // The piece to move should be a king
    LONG_CASTLE,     // The piece to move should be a king
    EN_PASSANT
};

struct Piece {
    PieceType type;
    bool isWhite;
};

struct BoardPos {
    char x;
    char y;
};

constexpr Square makeSquare(int x, int y) {
    return x + 8u*y;
}
constexpr Square makeSquare(const BoardPos &pos) {
    return pos.x + 8u*pos.y;
}
inline int squareX(Square square) {
    return square % 8u;
}
inline int squareY(Square square) {
    return square / 8u;
}
// Fast bitscan, removes and returns the last square/bit's index
inline Square popLastSquare(uint64_t &bitBoard) {
    uint64_t LS1B = bitBoard & (-bitBoard);
    bitBoard ^= LS1B;
    return _lzcnt_u64(LS1B);
}
// Fast bitscan, removes and returns the last bit
inline uint64_t popLastBit(uint64_t &bitBoard) {
    uint64_t LS1B = bitBoard & (-bitBoard);
    bitBoard ^= LS1B;
    return LS1B;
}
// Returns the square corresponding to the bit
inline uint64_t bitToSquare(uint64_t b) {
    return _lzcnt_u64(b);
}

struct Move {
    Square startSquare = 0;
    Square endSquare = 0;

    PieceType promotionType = EMPTY;
    MoveType moveType = NORMAL_MOVE;
};


const int ROW_COUNT = 8;
const int PIECE_TYPE_COUNT = 6;
const int PROMOTION_PIECES_COUNT = 4;


const int piecesCount[] = {
    8, // PAWN
    2, // BISHOP
    2, // KNIGHT
    2, // ROOK
    1, // QUEEN
    1  // KING
};


// From least to most valuable in material points (Bishop and Knights might be inverted)
const PieceType promotionTypes[] = {
    BISHOP,
    KNIGHT,
    ROOK,
    QUEEN
};
const PieceType pieceTypes[] = {
    PAWN,
    BISHOP,
    KNIGHT,
    ROOK,
    QUEEN,
    KING
};


inline bool isInBoard(int x, int y) {
    return 0 <= x && x < ROW_COUNT && 0 <= y && y < ROW_COUNT;
}

inline bool canPawnDoubleMove(int y, bool isWhite) {
    return (y == 1 && !isWhite) || (y == 6 && isWhite);
}
inline bool canPawnDoubleMove(uint64_t firstPushBit, bool isWhite) {
    return ((firstPushBit & 0b00000000'00000000'11111111'00000000'00000000'00000000'00000000'00000000ULL) && !isWhite) || 
           ((firstPushBit & 0b00000000'00000000'00000000'00000000'00000000'11111111'00000000'00000000ULL) && isWhite);
}
inline bool canPawnPromote(int y, bool isWhite) {
    return (y == 7 && !isWhite) || (y == 0 && isWhite);
}
inline bool canPawnPromote(uint64_t b, bool isWhite) {
    return ((b & 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'11111111ULL) && !isWhite) || 
           ((b & 0b11111111'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL) && isWhite);
}

const bool BLACK = false;
const bool WHITE = true;

const int FIFTY_MOVE_RULE_PLIES = 100;


constexpr Piece EMPTY_PIECE = {EMPTY, false};

/*const uint64_t whiteBitBoard[PIECE_TYPE_COUNT] = {
    // Pawn
    0b00000000'00000000'00000000'00000000'00000000'00000000'11111111'00000000ULL,
    // Bishop
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00100100ULL,
    // Knight
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'01000010ULL,
    // Rook
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'10000001ULL,
    // Queen
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00010000ULL,
    // King
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00001000ULL
};
const uint64_t blackBitBoard[PIECE_TYPE_COUNT] = {
    // Pawn
    0b00000000'11111111'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Bishop
    0b00100100'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Knight
    0b01000010'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Rook
    0b10000001'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Queen
    0b00010000'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // King
    0b00001000'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL
};*/

const uint64_t colorBitBoards[2][PIECE_TYPE_COUNT] = {
    {
    // Black bitboards
    // Pawn
    0b00000000'11111111'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Bishop
    0b00100100'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Knight
    0b01000010'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Rook
    0b10000001'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // Queen
    0b00010000'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // King
    0b00001000'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL
}, {
    // White bitboards
    // Pawn
    0b00000000'00000000'00000000'00000000'00000000'00000000'11111111'00000000ULL,
    // Bishop
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00100100ULL,
    // Knight
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'01000010ULL,
    // Rook
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'10000001ULL,
    // Queen
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00010000ULL,
    // King
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00001000ULL
}};


constexpr Piece defaultBoardPieces[64] = {
    {ROOK, BLACK}, {KNIGHT, BLACK}, {BISHOP, BLACK}, {QUEEN, BLACK}, {KING, BLACK}, {BISHOP, BLACK}, {KNIGHT, BLACK}, {ROOK, BLACK}, 
    {PAWN, BLACK}, {PAWN, BLACK},   {PAWN, BLACK},   {PAWN, BLACK},  {PAWN, BLACK}, {PAWN, BLACK},   {PAWN, BLACK},   {PAWN, BLACK},
    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,
    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,
    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,
    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE,    EMPTY_PIECE,   EMPTY_PIECE,     EMPTY_PIECE,     EMPTY_PIECE, 
    {PAWN, WHITE}, {PAWN, WHITE},   {PAWN, WHITE},   {PAWN, WHITE},  {PAWN, WHITE}, {PAWN, WHITE},   {PAWN, WHITE},   {PAWN, WHITE},
    {ROOK, WHITE}, {KNIGHT, WHITE}, {BISHOP, WHITE}, {QUEEN, WHITE}, {KING, WHITE}, {BISHOP, WHITE}, {KNIGHT, WHITE}, {ROOK, WHITE},
};


void printMove(const Move &move) {
    printf("Move (%d, %d) -> (%d, %d) ", move.startSquare % 8u, move.startSquare / 8u, move.endSquare % 8u, move.endSquare / 8u);
    if (move.moveType == EN_PASSANT) {
        printf("EnPassant ");
    } else if (move.moveType == SHORT_CASTLE) {
        printf("ShortCastle ");
    } else if (move.moveType == LONG_CASTLE) {
        printf("LongCastle ");
    }
    if (move.promotionType != EMPTY) {
        printf("Promotion ");
    }
    printf("\n");
}
void printPos(const BoardPos &pos) {
    printf("BoardPos (%d, %d)\n", pos.x, pos.y);
}

const bool colors[2] = {true, false};

constexpr Move NO_MOVE = {0, 0};

bool operator==(const BoardPos& a, const BoardPos& b) {
    return a.x == b.x && a.y == b.y;
}
bool operator==(const Move& a, const Move& b) {
    return a.startSquare == b.startSquare && a.endSquare == b.endSquare && a.promotionType == b.promotionType;
}
bool operator!=(const Move& a, const Move& b) {
    return a.startSquare != b.startSquare || a.endSquare != b.endSquare || a.promotionType != b.promotionType;
}


const uint64_t columnMasks[8] = {
    0x8080808080808080,
    0x4040404040404040,
    0x2020202020202020,
    0x1010101010101010,
    0x0808080808080808,
    0x0404040404040404,
    0x0202020202020202,
    0x0101010101010101
};

// Castle
const uint64_t shortCastleMasks[2] = {
    // Black
    0b00000110'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // White
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000110ULL,
};
const uint64_t longCastleMasks[2] = {
    // Black
    0b01110000'00000000'00000000'00000000'00000000'00000000'00000000'00000000ULL,
    // White
    0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'01110000ULL,
};

const char initialCastlingFlag = 0b1111;

const char SHORT_CASTLE_BLACK = 0b0001;
const char SHORT_CASTLE_WHITE = 0b0010;
const char LONG_CASTLE_BLACK = 0b0100;
const char LONG_CASTLE_WHITE = 0b1000;

const char CASTLE_BLACK = SHORT_CASTLE_BLACK | LONG_CASTLE_BLACK;
const char CASTLE_WHITE = SHORT_CASTLE_WHITE | LONG_CASTLE_WHITE;

const char CASTLE_COLOR[2] = {CASTLE_BLACK, CASTLE_WHITE};

const char shortCastleFlags[2] = {SHORT_CASTLE_BLACK, SHORT_CASTLE_WHITE};
const char longCastleFlags[2] = {LONG_CASTLE_BLACK, LONG_CASTLE_WHITE};

// initial position for rooks of short/long castle
constexpr Square shortRookSquares[2] = {
    makeSquare(7, 0),     // Black
    makeSquare(7, 7)      // White
};
constexpr Square longRookSquares[2] = {
    makeSquare(0, 0),     // Black
    makeSquare(0, 7)      // White
};
// Squares that need not to be attacked in order to castle
constexpr Square shortCastleCheckSquares[2][2] = {
    {makeSquare(5, 0), makeSquare(6, 0)},   // Black
    {makeSquare(5, 7), makeSquare(6, 7)}    // White
};
constexpr Square longCastleCheckSquares[2][2] = {
    {makeSquare(2, 0), makeSquare(3, 0)},   // Black
    {makeSquare(2, 7), makeSquare(3, 7)}    // White
};
// Final positions of castled king and rooks
constexpr Square shortCastleKingDestination[2] = {
    makeSquare(6, 0),     // Black
    makeSquare(6, 7)      // White
};
constexpr Square longCastleKingDestination[2] = {
    makeSquare(2, 0),     // Black
    makeSquare(2, 7)      // White
};
constexpr Square shortCastleRookDestination[2] = {
    makeSquare(5, 0),   // Black
    makeSquare(5, 7)    // White
};
constexpr Square longCastleRookDestination[2] = {
    makeSquare(3, 0),   // Black
    makeSquare(3, 7)    // White
};


uint64_t zobristPiecesSquaresColor[2][6][64];
uint64_t zobristWhiteMoves;
uint64_t zobristCastling[16];
uint64_t zobristEnPassantFiles[8];
uint64_t zobristDefaultGameHash;
void genZobristKeys() {
    // For debug
    random.seed(0);

    for (int i = 0 ; i < 6 ; i++) {
        for (int j = 0 ; j < 64 ; j++) {
            zobristPiecesSquaresColor[BLACK][i][j] = random();
            zobristPiecesSquaresColor[WHITE][i][j] = random();
        }
    }

    zobristWhiteMoves = random();

    for (int i = 0 ; i < 16 ; i++) {
        zobristCastling[i] = random();
    }

    for (int i = 0 ; i < 8 ; i++) {
        zobristEnPassantFiles[i] = random();
    }

    zobristDefaultGameHash = zobristWhiteMoves;
    for (int i = 0 ; i < 64 ; i++) {
        Piece piece = defaultBoardPieces[i];

        if (piece.type != EMPTY) {
            zobristDefaultGameHash ^= zobristPiecesSquaresColor[piece.isWhite][piece.type][i];
        }
    }
    zobristDefaultGameHash ^= zobristCastling[initialCastlingFlag];
}



// Bugged on some consoles...
/*const std::string piecesEmojiColor[2][PIECE_TYPE_COUNT] = {
    {"♙", "♗", "♘", "♖", "♕", "♔"},
    {"♟", "♝", "♞", "♜", "♛", "♚"}
};*/
const char piecesEmojiColor[2][PIECE_TYPE_COUNT] = {
    {'p', 'b', 'n', 'r', 'q', 'k'},
    {'P', 'B', 'N', 'R', 'Q', 'K'}
};


// Used for debug
inline uint64_t buildBitboard(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h) {
    return (a << 7*8) |
           (b << 6*8) |
           (c << 5*8) |
           (d << 4*8) |
           (e << 3*8) |
           (f << 2*8) |
           (g << 1*8) |
           (h << 0*8);
}


#endif
