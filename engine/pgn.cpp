#include "board.cpp"
#include "botConstants.cpp"


// Only accepts default starting position
// VERY clunky + Work In Progress
// Mainly for very simple pgn reading and to speed up NNUE data extraction


const char * longCastleSAN = "O-O-O";
const char * shortCastleSAN = "O-O";


void _skipVariationsAndComments(const std::string_view pgnString, int &index) {
    int length = pgnString.size();
    int parenthesis = 0;
    int braces = 0;
    do {
        switch (pgnString[index]) {
        case '(':
            parenthesis += 1;
            break;
        case '{':
            braces += 1;
            break;
        case ')':
            parenthesis -= 1;
            break;
        case '}':
            braces -= 1;
            break;
        }
        index += 1;
    } while ((braces > 0 || parenthesis > 0) && (index < length));
}
// Returns EMPTY if this isn't the start of a move
inline PieceType _sanMovePiece(char c) {
    switch (c) {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'P':
            return PAWN;
        case 'B':
            return BISHOP;
        case 'N':
            return KNIGHT;
        case 'R':
            return ROOK;
        case 'Q':
            return QUEEN;
        case 'K':
        case 'O':
            return KING;
        default:
            return EMPTY;
    }
}
inline bool isRow(char c) { return ('1' <= c) && (c <= '8'); }
inline bool isColumn(char c) { return ('a' <= c) && (c <= 'h'); }
inline bool isTakes(char c) { return c == 'x'; }

// Better performances for high scale imports
// Ignores all headers !
void extractPGN_Moves(const std::string_view &pgnString, std::vector<Move> &moves) {
    const int length = pgnString.size();
    int index = 0;

    // Strip headers
    while (index < length && (pgnString[index] == '[' || pgnString[index] == '\n')) {
        index = pgnString.find('\n', index);

        if (index == std::string_view::npos) {
            throw std::invalid_argument(std::format("Header Strip - Unexpected EOF in PGN\n{}\n", pgnString));
        }
        index += 1;
    }

    // Parse moves
    /*int moveNumber = 1;
    std::string moveNumberText = std::format("{}.", moveNumber);*/
    Board board = {};

    while (index < length) {
        char currentChar = pgnString[index];

        if ((currentChar == '(') || (currentChar == '{')) {
            _skipVariationsAndComments(pgnString, index);
            continue;
        }

        if (currentChar == 'O') {
            // Castling moves
            if (pgnString.substr(index, 5) == longCastleSAN) {
                Move move;
                if (board.whiteTurn) {
                    move = {board.whiteKingSquare, longCastleKingDestination[WHITE], EMPTY, LONG_CASTLE};
                } else {
                    move = {board.blackKingSquare, longCastleKingDestination[BLACK], EMPTY, LONG_CASTLE};
                }

                board.playMove(move, false);
                moves.push_back(move);
                index += 5;
                continue;                
            } else if (pgnString.substr(index, 3) == shortCastleSAN) {
                Move move;
                if (board.whiteTurn) {
                    move = {board.whiteKingSquare, shortCastleKingDestination[WHITE], EMPTY, SHORT_CASTLE};
                } else {
                    move = {board.blackKingSquare, shortCastleKingDestination[BLACK], EMPTY, SHORT_CASTLE};
                }

                board.playMove(move, false);
                moves.push_back(move);
                index += 3;
                continue;
            } else {
                index += 1;
                continue;
            }
        }

        PieceType pieceType = _sanMovePiece(currentChar);
        if (pieceType != EMPTY) {
            // std::cout << pgnString.substr(index, 5) << std::endl;

            // Move decoding
            if (pieceType != PAWN) {
                index += 1;
            }
            if (isTakes(pgnString[index])) { index += 1; }

            // Full by default
            uint64_t selectionMask = -1;

            Square endSquare;
            char c = pgnString[index];

            if (isColumn(c) && isRow(pgnString[index+1])) {
                if (isColumn(pgnString[index+2]) || isTakes(pgnString[index+2])) {
                    // Ne2c3
                    int startColumn = c - 'a';
                    int startRow = 8 - (pgnString[index+1] - '0');
                    selectionMask = (0x8080808080808080ULL >> startColumn) & (0xff00000000000000ULL >> (startRow * 8));
                    
                    index += 2;
                    if (isTakes(pgnString[index])) { index += 1; }
                    
                    endSquare = makeSquare(pgnString[index] - 'a', 8 - (pgnString[index+1] - '0'));
                    index += 2;
                } else {
                    // Ne3
                    endSquare = makeSquare(c - 'a', 8 - (pgnString[index+1] - '0'));
                    index += 2;
                }
            } else if (isColumn(c)) {
                // Nde3
                int startColumn = c - 'a';
                selectionMask = 0x8080808080808080ULL >> startColumn;

                index += 1;
                if (isTakes(pgnString[index])) { index += 1; }

                endSquare = makeSquare(pgnString[index] - 'a', 8 - (pgnString[index+1] - '0'));
                index += 2;
            } else if (isRow(c)) {
                // N2e3
                int startRow = 8 - (c - '0');
                selectionMask = 0xff00000000000000ULL >> (startRow * 8);

                index += 1;
                if (isTakes(pgnString[index])) { index += 1; }

                endSquare = makeSquare(pgnString[index] - 'a', 8 - (pgnString[index+1] - '0'));
                index += 2;
            } else {
                throw std::invalid_argument(std::format("Invalid PGN (can't decode move)\n{}\n", pgnString));
            }

            PieceType promotionType = EMPTY;
            if (pgnString[index] == '=') {
                promotionType = _sanMovePiece(pgnString[index+1]);
                index += 2;
            }

            while ((pgnString[index] != ' ') && (pgnString[index] != '\n') && (index < length)) {
                index += 1;
            }

            // Find the move
            uint64_t moveMask = board.attacksMask(endSquare, pieceType, !board.whiteTurn);
            // Pawn moves are added in the attack mask
            if (pieceType == PAWN) {
                Piece endPiece = board.getAt(endSquare);
                if (endPiece.type == EMPTY || endPiece.isWhite != !board.whiteTurn) {
                    if ((board.enPassantBB & bit(endSquare)) == 0) {
                        moveMask = 0;
                    }
                }

                uint64_t pushBit;
                if (board.whiteTurn) {
                    pushBit = bit(endSquare + 8u);

                    if (((board.allOccupancy & pushBit) == 0) && ((pushBit & 0x0000000000ff0000ULL) != 0)) {
                        pushBit |= pushBit >> 8u;
                    }
                } else {
                    pushBit = bit(endSquare - 8u);

                    if (((board.allOccupancy & pushBit) == 0) && ((pushBit & 0x0000ff0000000000ULL) != 0)) {
                        pushBit |= pushBit << 8u;
                    }
                }
                moveMask |= pushBit;
            }

            /*printBB(moveMask);
            printBB(board.colorBB[board.whiteTurn][pieceType]);
            printBB(selectionMask);*/

            uint64_t startBit = board.colorBB[board.whiteTurn][pieceType] &
                                selectionMask &
                                moveMask;
            if (startBit == 0) {
                throw std::invalid_argument(std::format("Invalid PGN (can't find startSquare)\n{}\n", pgnString));
            }
            
            /*printBB(startBit);
            printf("%d\n", pieceType);*/
            /*if (endSquare > 63 || endSquare < 0) {
                printf("!!! %d\n", endSquare);
                printf("%d\n", index);
                std::cout << pgnString.substr(index, 20) << std::endl;
                exit(-1);
            }*/

            Square startSquare;
            Move move;
            if (std::popcount(startBit) > 1) {
                // Choose the only one that doen't produce an illegal position
                bool ok = false;
                while (startBit) {
                    startSquare = popLastSquare(startBit);
                    move = {startSquare, endSquare, promotionType};

                    if (!board.leadsToCheck(move)) {
                        ok = true;
                        break;
                    }
                }
                if (!ok) {
                    throw std::invalid_argument(std::format("Invalid PGN (can't find startSquare among possibilities)\n{}\n", pgnString));
                }
            } else {
                // There is only one possibility
                startSquare = bitToSquare(startBit);
                move = {startSquare, endSquare, promotionType};
            }

            if (pieceType == PAWN && (squareX(startSquare) != squareX(endSquare)) && (board.getAt(endSquare).type == EMPTY)) {
                move.moveType = EN_PASSANT;
            }

            board.playMove(move, false);
            moves.push_back(move);

            /*printMove(move);
            printBoard(board);
            printf("\n");*/

            /*for (int x = 0 ; x < 8 ; x++) {
                for (int y = 0 ; y < 8 ; y++) {
                    Piece a = board.getAt(x, y);
                    Piece b = board.getAtDebug(x, y);
                    if (a.type != b.type) {
                        printf("Different type !!! at %d %d\n", x, y);
                        printf("%d vs %d   %d vs %d\n", a.type, b.type, a.isWhite, b.isWhite);
                        printf("%d\n", index);
                        printf("%d %d %d %d\n", move.startSquare, move.endSquare, move.promotionType, move.moveType);
                        std::cout << pgnString.substr(index, 20) << std::endl;

                        for (int color = 0 ; color < 2 ; color++) {
                            for (int i = 0 ; i < 6 ; i++) {
                                printBB(board.colorBB[color][i]);
                            }
                        }
                        exit(-1);
                    }

                    if (board.occupencies[WHITE] & board.occupencies[BLACK]) {
                        printf("Conflict !!!\n");
                        exit(-1);
                    }
                }
            }*/

        } else {
            index += 1;
        }
    }
}


// Assumes Every move is evaluated and that it's written as `e6 { [%eval 0.27] }`
// The scores are generally absolute (from white's perspective)
void extractPGN_Scores(const std::string_view &pgnString, std::vector<int> &scores) {
    const int length = pgnString.size();

    int index = 0;
    while (index < length) {
        index = pgnString.find("[%eval", index);

        if (index == std::string_view::npos) {
            break;
        }

        index += 6;
        // Can maybe remove that
        if (pgnString[index] == ' ') {
            [[likely]]
            index += 1;
        }

        int scoreStartIndex = index;
        index = pgnString.find("]", index);

        if (index == std::string_view::npos) {
            throw std::invalid_argument(std::format("Score extraction - Unclosed bracket in PGN\n{}\n", pgnString)); 
        }

        std::string_view scoreString = pgnString.substr(scoreStartIndex, index - scoreStartIndex);

        if (scoreString[0] == '#') {
            // Absolute mate distance (signed)
            int mateDistance;
            auto [ptr, ec] = std::from_chars(scoreString.data() + 1, scoreString.data() + scoreString.size(), mateDistance);

            if (ptr != scoreString.data() + scoreString.size()) {
                throw std::invalid_argument(std::format("Score extraction - Invalid score in PGN\n{}\n", pgnString));
            }

            if (mateDistance > 0) {
                scores.push_back(CHECKMATE_BASE_SCORE - mateDistance);
            } else {
                scores.push_back(-CHECKMATE_BASE_SCORE - mateDistance);
            }
        } else {
            // Absolute score
            float score;
            auto [ptr, ec] = std::from_chars(scoreString.data(), scoreString.data() + scoreString.size(), score);

            if (ptr != scoreString.data() + scoreString.size()) {
                throw std::invalid_argument(std::format("Score extraction - Invalid score in PGN\n{}\n", pgnString));
            }

            scores.push_back(std::lround(score * 100.0f));
        }
    }
}


// Assumes Every move is evaluated and that it's written as `e6 { [%eval 0.27] }`
// The scores are generally absolute (from white's perspective)
// If the last move is a checkmate, the last move doesn't isn't scored
void extractPGN_MovesScores(const std::string_view &pgnString, std::vector<Move> &moves, std::vector<int> &scores) {
    extractPGN_Moves(pgnString, moves);
    extractPGN_Scores(pgnString, scores);

    if (moves.size() != scores.size() && moves.size() != scores.size()+1) {
        throw std::range_error(std::format("Score and move count don't match in PGN ({} vs {})\n{}\n", moves.size(), scores.size(), pgnString));
    }
}


/*struct FullPGN_Game {
    
};*/


