#include "board.cpp"


struct FenCoordinateChars {
    char letter;
    char digit;
};

// FEN Helper functions

inline Piece fenCharToPiece(char c) {
    switch (c) {
        case 'P': return {PAWN, WHITE};
        case 'B': return {BISHOP, WHITE};
        case 'N': return {KNIGHT, WHITE};
        case 'R': return {ROOK, WHITE};
        case 'Q': return {QUEEN, WHITE};
        case 'K': return {KING, WHITE};

        case 'p': return {PAWN, BLACK};
        case 'b': return {BISHOP, BLACK};
        case 'n': return {KNIGHT, BLACK};
        case 'r': return {ROOK, BLACK};
        case 'q': return {QUEEN, BLACK};
        case 'k': return {KING, BLACK};
    }
    return EMPTY_PIECE;
}

inline Square fenCoordinateToSquare(char letter, char digit) {
    return makeSquare(letter - 'a', 8 - (digit - '0'));
}

inline FenCoordinateChars squareToFenCoordinate(Square square) {
    return {(char) ('a' + squareX(square)), (char) ('8' - squareY(square))};
}


Board loadFEN(const std::string fenString) {
    uint64_t colorBitBoards[2][PIECE_TYPE_COUNT] = {};
    char castlingFlag = 0;
    bool whiteTurn;
    uint64_t enPassantBB = 0;

    int index = 0;
    for (int y = 0 ; y < 8 ; y++) {
        int x = 0;

        while (x < 8) {
            char c = fenString[index];
            if (std::isdigit(c)) {
                int emptySquares = c - '0';
                x += emptySquares;
                index += 1;
            } else {
                Piece piece = fenCharToPiece(c);

                if (piece.type == EMPTY) {
                    printf("Board parsing - Unexpected symbol '%c' at position %d while reading FEN:\n", c, index);
                    std::cout << fenString << std::endl;
                    printf("x=%d, y=%d\n", x, y);
                    throw std::invalid_argument(std::format("Board parsing - Unexpected symbol '{}' at position {} while reading FEN:\n", c, index));
                }

                colorBitBoards[piece.isWhite][piece.type] |= bit(x, y);

                x += 1;
                index += 1;
            }
        }
        index += 1;
    }

    char sideToMoveChar = fenString[index];
    if (sideToMoveChar == 'w') {
        whiteTurn = WHITE;
    } else if (sideToMoveChar == 'b') {
        whiteTurn = BLACK;
    } else {
        printf("Turn parsing - Unexpected symbol '%c' at position %d while reading FEN:\n", sideToMoveChar, index);
        std::cout << fenString << std::endl;
        throw std::invalid_argument(std::format("Turn parsing - Unexpected symbol '{}' at position {} while reading FEN:\n", sideToMoveChar, index));
    }
    index += 2;

    for (int i = 0 ; i < 5 ; i++) {
        char castleRight = fenString[index];

        if (castleRight == 'K')      { castlingFlag |= SHORT_CASTLE_WHITE; }
        else if (castleRight == 'Q') { castlingFlag |= LONG_CASTLE_WHITE;  }
        else if (castleRight == 'k') { castlingFlag |= SHORT_CASTLE_BLACK; }
        else if (castleRight == 'q') { castlingFlag |= LONG_CASTLE_BLACK;  }
        else if (castleRight == '-') {}
        else if (castleRight == ' ') {
            index += 1;
            break;
        }
        else {
            printf("Castling rights - Unexpected symbol '%c' at position %d while reading FEN:\n", castleRight, index);
            std::cout << fenString << std::endl;
            throw std::invalid_argument(std::format("Castling rights - Unexpected symbol '{}' at position {} while reading FEN:\n", castleRight, index));
        }
        index += 1;
    }

    if (fenString[index] == '-') {
        index += 2;
    } else {
        Square enPassantSquare = fenCoordinateToSquare(fenString[index], fenString[index+1]);
        enPassantBB = bit(enPassantSquare);
        index += 3;
    }

    // TODO : Half move clock and full move clock are currently not used

    return loadBoard(colorBitBoards, castlingFlag, whiteTurn, enPassantBB);
}
