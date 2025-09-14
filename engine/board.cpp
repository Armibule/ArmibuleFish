#ifndef BOARD
#define BOARD

#include "gameConstants.cpp"
#include "botConstants.cpp"
#include <vector>
#include <iostream>
#include <fstream>


// Contains the information to unmake a move
struct UnmakeMoveInfo {
    Piece capturedPiece;
    int previousClockResetIndex;
    uint64_t previousZobristHash;
    char castlingFlag;
    GameState previousState;
    uint64_t previousEnPassantBB;

    int previousPieceSquareScoreOpening;
    int previousPieceSquareScoreEndgame;
};



GamePhase getGamePhase(const int whitePieces[PIECE_TYPE_COUNT], const int blackPieces[PIECE_TYPE_COUNT]) {
    // Should range from 0 to 256  (16*4 + 4*14 + 2*24 + 2*44 = 256)
    return 256 
            - (whitePieces[PAWN] + blackPieces[PAWN]) * 3
            - (whitePieces[BISHOP] + whitePieces[KNIGHT] + blackPieces[BISHOP] + blackPieces[KNIGHT]) * 10
            - (whitePieces[ROOK] + blackPieces[ROOK]) * 16
            - (whitePieces[QUEEN] + blackPieces[QUEEN]) * 32;
}


class Board {
    public:
        Board(bool whiteTurn, 
              const uint64_t colorBitBoards[2][PIECE_TYPE_COUNT],
              uint64_t enPassantBB,
              Square whiteKingSquare, Square blackKingSquare, 
              GameState state, GamePhase phase, 
              const int whitePieces[PIECE_TYPE_COUNT], const int blackPieces[PIECE_TYPE_COUNT],
              const Piece pieces[64],
              char castlingFlag,
              uint64_t zobristHash, const std::vector<uint64_t> &previousHashes, int clockResetIndex) {
            
            this->whiteTurn = whiteTurn;

            std::copy(&colorBitBoards[0][0], &colorBitBoards[0][0] + 2*PIECE_TYPE_COUNT, &this->colorBB[0][0]);
            this->enPassantBB = enPassantBB;

            this->whiteKingSquare = whiteKingSquare;
            this->blackKingSquare = blackKingSquare;

            this->state = state;
            this->phase = phase;

            std::copy(whitePieces, whitePieces + PIECE_TYPE_COUNT, this->whitePieces);
            std::copy(blackPieces, blackPieces + PIECE_TYPE_COUNT, this->blackPieces);

            std::copy(pieces, pieces + 64, this->pieces);

            this->castlingFlag = castlingFlag;

            this->zobristHash = zobristHash;
            this->previousHashes = previousHashes;
            this->clockResetIndex = clockResetIndex;
        
            calcOccupancy();
            recomputePieceSquareScores();
        }
        Board() : Board(
            true, 
            colorBitBoards, 
            0ULL,
            4 + 8*7, 4 + 8*0, 
            NEUTRAL, INITIAL_GAME_PHASE, 
            piecesCount, piecesCount, 
            defaultBoardPieces, 
            initialCastlingFlag,
            /*false, 0.0f,*/
            zobristDefaultGameHash, {}, 0
        ) {}

        bool whiteTurn;
        
        uint64_t colorBB[2][PIECE_TYPE_COUNT];
        uint64_t enPassantBB;

        Square whiteKingSquare;
        Square blackKingSquare;

        GameState state;
        GamePhase phase;

        int whitePieces[PIECE_TYPE_COUNT];
        int blackPieces[PIECE_TYPE_COUNT];

        uint64_t occupencies[2];
        uint64_t allOccupancy;

        Piece pieces[64];

        // Tell if the short/long castles can be used in the future
        char castlingFlag;

        uint64_t zobristHash;
        std::vector<uint64_t> previousHashes;  // Previous game hashes since the last clear
        int clockResetIndex;    // Index of the first hash since the last capture/pawn move

        // Incremental update of piece square tables, used by the bot
        int pieceSquareScoreOpening;
        int pieceSquareScoreEndgame;

        // Use this function as rarely as possible, prefer unmaking moves
        Board copy() const {
            return Board(
                whiteTurn, colorBB, enPassantBB,
                whiteKingSquare, blackKingSquare, 
                state, phase, 
                whitePieces, blackPieces, pieces, 
                castlingFlag,
                zobristHash, previousHashes, clockResetIndex);
        }

        void pieceMoves(Square square, PieceType pieceType, bool isWhite, std::vector<Move> &moves) {
            uint64_t attacksMask = capturesMask(square, pieceType, isWhite);

            if (pieceType == PAWN) {
                // En passant
                if (attacksMask & enPassantBB) {
                    Square enPassantSquare = bitToSquare(enPassantBB);
                    Move move = {square, enPassantSquare, EMPTY, EN_PASSANT};
                    if (!leadsToCheck(move)) {
                        moves.push_back(move);
                    }
                }

                attacksMask &= occupencies[!isWhite];

                Square pushSquare;
                if (isWhite) {
                    pushSquare = square - 8u;
                } else {
                    pushSquare = square + 8u;
                }

                uint64_t b = bit(pushSquare);

                if (!(b & allOccupancy)) {
                    Move move = {square, pushSquare};

                    if (!leadsToCheck(move)) {
                        if (canPawnPromote(b, isWhite)) {
                            // Pawn promotions
                            moves.push_back({square, move.endSquare, BISHOP});
                            moves.push_back({square, move.endSquare, KNIGHT});
                            moves.push_back({square, move.endSquare, ROOK});
                            moves.push_back({square, move.endSquare, QUEEN});
                        } else {
                            moves.push_back(move);
                        }
                    }

                    if (canPawnDoubleMove(b, isWhite)) {
                        if (isWhite) {
                            pushSquare = square - 16u;
                        } else {
                            pushSquare = square + 16u;
                        }

                        b = bit(pushSquare);

                        if (!(b & allOccupancy)) {
                            move = {square, pushSquare};

                            if (!leadsToCheck(move)) {
                                moves.push_back(move);
                            }
                        }
                    }
                }
            } else if (pieceType == KING) {
                // Short castle
                if (castlingFlag & shortCastleFlags[isWhite]) {
                    // Check if squares are empty
                    if (!(allOccupancy & shortCastleMasks[isWhite]) && !isInCheck(whiteTurn)) {
                        // Check if all squares are free of attack
                        bool ok = true;
                        for (const Square checkSquare : shortCastleCheckSquares[isWhite]) {
                            Move checkMove = {square, checkSquare};
                            if (leadsToCheck(checkMove)) {
                                ok = false;
                                break;
                            }
                        }

                        if (ok) {
                            moves.push_back({square, shortCastleKingDestination[isWhite], EMPTY, SHORT_ROQUE});
                        }
                    }
                }
                // Long castle
                if (castlingFlag & longCastleFlags[isWhite]) {
                    // Check if squares are empty
                    if (!(allOccupancy & longCastleMasks[isWhite]) && !isInCheck(whiteTurn)) {
                        // Check if all squares are free of attack
                        bool ok = true;
                        for (const Square checkSquare : longCastleCheckSquares[isWhite]) {
                            Move checkMove = {square, checkSquare};
                            if (leadsToCheck(checkMove)) {
                                ok = false;
                                break;
                            }
                        }

                        if (ok) {
                            moves.push_back({square, longCastleKingDestination[isWhite], EMPTY, LONG_ROQUE});
                        }
                    }
                }
            }

            // No use of vectors, bitscan
            while (attacksMask) {
                Square bitSquare = popLastSquare(attacksMask);

                Move move = {square, bitSquare};

                if (!leadsToCheck(move)) {
                    if (pieceType == PAWN && canPawnPromote(squareY(bitSquare), isWhite)) {
                        // Pawn promotions
                        moves.push_back({square, bitSquare, QUEEN});
                        moves.push_back({square, bitSquare, ROOK});
                        moves.push_back({square, bitSquare, KNIGHT});
                        moves.push_back({square, bitSquare, BISHOP});
                    } else {
                        moves.push_back(move);
                    }
                }
            }
        }
        // Only for hasLegalMove method
        bool hasPieceMoves(Square square, PieceType pieceType, bool isWhite) {
            if (pieceType == PAWN) {
                Square pushSquare;
                if (isWhite) {
                    pushSquare = square - 8;
                } else {
                    pushSquare = square + 8;
                }

                uint64_t b = bit(pushSquare);

                if (!(b & allOccupancy)) {
                    Move move = {square, pushSquare};

                    if (!leadsToCheck(move)) {
                        return true;
                    }

                    if (canPawnDoubleMove(b, isWhite)) {
                        if (isWhite) {
                            pushSquare = square - 16u;
                        } else {
                            pushSquare = square + 16u;
                        }

                        b = bit(pushSquare);

                        if (!(b & allOccupancy)) {
                            move = {square, pushSquare};

                            if (!leadsToCheck(move)) {
                                return true;
                            }
                        }
                    }
                }
            }
            // Casteling doesn't need to be checked as it implies the king can already move

            // Computed after the rest 
            uint64_t attacksMask = capturesMask(square, pieceType, isWhite);

            if (pieceType == PAWN) {
                // En passant
                if (attacksMask & enPassantBB) {
                    Square enPassantSquare = bitToSquare(enPassantBB);
                    if (!leadsToCheck({square, enPassantSquare, EMPTY, EN_PASSANT})) {
                        return true;
                    }
                }

                attacksMask &= occupencies[!isWhite];
            }

            while (attacksMask) {
                Square bitSquare = popLastSquare(attacksMask);

                Move move = {square, bitSquare};

                if (!leadsToCheck(move)) {
                    return true;
                }
            }
            return false;
        }
        // Optimized to only give captures
        void pieceCaptureMoves(Square square, PieceType pieceType, bool isWhite, std::vector<Move> &moves) {
            uint64_t attacksMask = capturesMask(square, pieceType, isWhite);

            if (pieceType == PAWN) {
                // En passant
                if (attacksMask & enPassantBB) {
                    Square enPassantSquare = bitToSquare(enPassantBB);
                    Move move = {square, enPassantSquare, EMPTY, EN_PASSANT};
                    if (!leadsToCheck(move)) {
                        moves.push_back(move);
                    }
                }
            }

            // ANDs the attack mask with opponent's pieces to only have captures
            attacksMask = attacksMask & occupencies[!isWhite];

            // No use of vectors
            while (attacksMask) {
                Square bitSquare = popLastSquare(attacksMask);

                Move move = {square, bitSquare};

                if (!leadsToCheck(move)) {
                    if (pieceType == PAWN && canPawnPromote(squareY(bitSquare), isWhite)) {
                        // Pawn promotions
                        moves.push_back({square, bitSquare, QUEEN});
                        moves.push_back({square, bitSquare, ROOK});
                        moves.push_back({square, bitSquare, KNIGHT});
                        moves.push_back({square, bitSquare, BISHOP});
                    } else {
                        moves.push_back(move);
                    }
                }
            }
        }

        void getAllMoves(std::vector<Move> &moves) {
            if (state != NEUTRAL) {
                return;
            }
            /*if (areMovesComputed) {
                moves.reserve(allMoves.size());
                for (const Move &move : allMoves) {
                    moves.push_back(move);
                }
                return;
            }*/

            moves.reserve(50);

            uint64_t piecesOccupency = occupencies[whiteTurn];

            // Bitscan
            while (piecesOccupency) {
                Square bitSquare = popLastSquare(piecesOccupency);

                Piece piece = getAt(bitSquare);

                pieceMoves(bitSquare, piece.type, whiteTurn, moves);
            }

            /*moves.reserve(allMoves.size());
            for (const Move &move : allMoves) {
                moves.push_back(move);
            }*/

            // areMovesComputed = true;
        }
        // Only generate capturing moves, but not cached
        void getAllCaptureMoves(std::vector<Move> &moves) {
            if (state != NEUTRAL) {
                return;
            }
            /*if (areMovesComputed) {
                for (const Move &move : allMoves) {
                    if (isCapture(move)) {
                        moves.push_back(move);
                    }
                }
                return;
            }*/

            moves.reserve(30);

            uint64_t piecesOccupency = occupencies[whiteTurn];

            // Bitscan
            uint64_t LS1B;
            Square bitSquare;
            while (piecesOccupency) {
                bitSquare = popLastSquare(piecesOccupency);

                Piece piece = getAt(bitSquare);

                pieceCaptureMoves(bitSquare, piece.type, whiteTurn, moves);
            }
        }
        // Faster than getting all the moves
        bool hasLegalMove() {
            if (state != NEUTRAL) {
                return false;
            }
            /*if (areMovesComputed) {
                return allMoves.size() > 0;
            }*/

            uint64_t piecesOccupency = occupencies[whiteTurn];

            // Bitscan
            while (piecesOccupency) {
                Square bitSquare = popLastSquare(piecesOccupency);

                Piece piece = getAt(bitSquare);

                if (hasPieceMoves(bitSquare, piece.type, whiteTurn)) {
                    return true;
                }
            }

            return false;
        }

        bool leadsToCheck(const Move &move) {
            /*
            Piece movedPiece = getAt(move.startPos);
            Board boardCopy = copy();
            boardCopy.playMove(move, true, false);
            return boardCopy.isInCheck(whiteTurn);
            */

            // Stores the previous state
            uint64_t normalOccupencies[2] {
                occupencies[0],
                occupencies[1]
            };

            // Do legality check move by temporarly editing the occupency
            Piece piece = getAt(move.startSquare);

            uint64_t startBit = bit(move.startSquare);
            uint64_t endBit = bit(move.endSquare);

            // If there is a castle, the rooks can't protect the king so we don't need to move them
            // (they are at the corner of the board)

            if (move.moveType == EN_PASSANT) {
                // Clear both sides to prevent more branching, removes captured pawn
                occupencies[!whiteTurn] &= ~((startBit << 8ULL) | (startBit >> 8ULL));
            }

            occupencies[whiteTurn] &= ~startBit;
            occupencies[whiteTurn] |= endBit;
            occupencies[!whiteTurn] &= ~endBit;

            allOccupancy = occupencies[BLACK] | occupencies[WHITE];

            bool isChecked;
            if (piece.type == KING) {
                isChecked = leadsToCheck_isInCheck(move.endSquare);
            } else {
                if (whiteTurn) {
                    isChecked = leadsToCheck_isInCheck(whiteKingSquare);
                } else {
                    isChecked = leadsToCheck_isInCheck(blackKingSquare);
                }
            }

            // Restores previous state
            occupencies[0] = normalOccupencies[0];
            occupencies[1] = normalOccupencies[1];
            allOccupancy = occupencies[0] | occupencies[1];

            return isChecked;
        }
        // Only to use in leadsToCheck function
        bool leadsToCheck_isInCheck(Square kingSquare) {
            uint64_t * otherBB;

            otherBB = colorBB[!whiteTurn];

            // Maybe faster ?
            if (capturesMask(kingSquare, KNIGHT, whiteTurn) & otherBB[KNIGHT]) { return true; }
            if (capturesMask(kingSquare, PAWN, whiteTurn) & otherBB[PAWN]) { return true; }
            if (capturesMask(kingSquare, QUEEN, whiteTurn) & (otherBB[KING] | otherBB[BISHOP] | otherBB[ROOK] | otherBB[QUEEN])) {
                if (capturesMask(kingSquare, KING, whiteTurn) & otherBB[KING]) { return true; }
                if (capturesMask(kingSquare, BISHOP, whiteTurn) & (otherBB[BISHOP] | otherBB[QUEEN])) { return true; }
                if (capturesMask(kingSquare, ROOK, whiteTurn) & (otherBB[ROOK] | otherBB[QUEEN])) { return true; }
            }
            return false;
        }

        bool isInCheck(bool isWhite) {
            uint64_t * otherBB;
            Square kingSquare;
            otherBB = colorBB[!isWhite];
            if (isWhite) {
                if (isWhiteCheckComputed) {
                    return isWhiteInCheck;
                }

                kingSquare = whiteKingSquare;

                isWhiteCheckComputed = true;
                isWhiteInCheck = true;
            } else {
                if (isBlackCheckComputed) {
                    return isBlackInCheck;
                }

                kingSquare = blackKingSquare;

                isBlackCheckComputed = true;
                isBlackInCheck = true;
            }

            if (capturesMask(kingSquare, KNIGHT, isWhite) & otherBB[KNIGHT]) { return true; }
            if (capturesMask(kingSquare, PAWN, isWhite) & otherBB[PAWN]) { return true; }
            if (capturesMask(kingSquare, QUEEN, isWhite) & (otherBB[KING] | otherBB[BISHOP] | otherBB[ROOK] | otherBB[QUEEN])) {
                if (capturesMask(kingSquare, KING, isWhite) & otherBB[KING]) { return true; }
                if (capturesMask(kingSquare, BISHOP, isWhite) & (otherBB[BISHOP] | otherBB[QUEEN])) { return true; }
                if (capturesMask(kingSquare, ROOK, isWhite) & (otherBB[ROOK] | otherBB[QUEEN])) { return true; }
            }

            if (isWhite) {
                isWhiteInCheck = false;
            } else {
                isBlackInCheck = false;
            }

            return false;
        }

        // Displays using bitboards - For debug
        /*Piece getAt(int x, int y) const {
            uint64_t b = bit(x, y);
            // whiteOccupancy
            if (b & occupencies[WHITE] ) {  
                if (b & colorBB[WHITE][PAWN]) { return {PAWN, true}; }
                if (b & colorBB[WHITE][BISHOP]) { return {BISHOP, true}; }
                if (b & colorBB[WHITE][KNIGHT]) { return {KNIGHT, true}; }
                if (b & colorBB[WHITE][ROOK]) { return {ROOK, true}; }
                if (b & colorBB[WHITE][QUEEN]) { return {QUEEN, true}; }
                if (b & colorBB[WHITE][KING]) { return {KING, true}; }
            // blackOccupancy
            } else if (b & occupencies[BLACK]) {
                if (b & colorBB[BLACK][PAWN]  ) { return {PAWN, false}; }
                if (b & colorBB[BLACK][BISHOP]) { return {BISHOP, false}; }
                if (b & colorBB[BLACK][KNIGHT]) { return {KNIGHT, false}; }
                if (b & colorBB[BLACK][ROOK]  ) { return {ROOK, false}; }
                if (b & colorBB[BLACK][QUEEN] ) { return {QUEEN, false}; }
                if (b & colorBB[BLACK][KING]  ) { return {KING, false}; }
            }
            return {EMPTY, false};
        }*/
        // Displays efficiently
        Piece getAt(Square square) const {
            return pieces[square];
        }
        Piece getAt(int x, int y) const {
            return pieces[x + 8u*y];
        }
        Piece getAt(const BoardPos &pos) const {
            // return getAt(pos.x, pos.y);
            return pieces[pos.x + 8u*pos.y];
        }

        // To call BEFORE the move is played
        bool isCapture(const Move &move) const {
            return getAt(move.endSquare).type != EMPTY || move.moveType == EN_PASSANT;
        }

        // Returns the information to unmake the move
        // TODO : Could have bugs with zobrist hashing, in particular with en passant
        UnmakeMoveInfo playMove(const Move &move) {
            Square startSquare = move.startSquare;
            Square endSquare = move.endSquare;

            Piece piece = getAt(startSquare);

            uint64_t startBit = bit(startSquare);
            uint64_t endBit = bit(endSquare);

            Square capturedSquare = endSquare;
            uint64_t capturedBit = endBit;
            Piece destPiece;
            if (move.moveType == EN_PASSANT) {
                if (whiteTurn) {
                    capturedSquare += 8;
                    capturedBit >>= 8ULL;
                } else {
                    capturedSquare -= 8;
                    capturedBit <<= 8ULL;
                }
                destPiece = getAt(capturedSquare);
                pieces[capturedSquare] = EMPTY_PIECE;
            } else {
                destPiece = getAt(capturedSquare);
            }

            UnmakeMoveInfo info = {
                destPiece, 
                clockResetIndex, 
                zobristHash, 
                castlingFlag, 
                state,
                enPassantBB,
                pieceSquareScoreOpening,
                pieceSquareScoreEndgame
            };

            zobristHash ^= zobristCastling[castlingFlag];

            if (enPassantBB) {
                // Clears en passant BB
                Square enPassantSquare = bitToSquare(enPassantBB);
                zobristHash ^= zobristEnPassantFiles[squareX(enPassantSquare)];

                enPassantBB = 0ULL;
            }

            // Removes the captured piece
            if (destPiece.type != EMPTY) {
                removePieceSquareScore(!whiteTurn, destPiece.type, capturedSquare);
                zobristHash ^= zobristPiecesSquaresColor[!whiteTurn][destPiece.type][capturedSquare];
                colorBB[!whiteTurn][destPiece.type] &= ~capturedBit;
                if (whiteTurn) {
                    blackPieces[destPiece.type] -= 1;
                } else {
                    whitePieces[destPiece.type] -= 1;
                }

                if (destPiece.type == ROOK) {
                    // Can't castle with a taken rook
                    if (capturedSquare == shortRookSquares[!whiteTurn]) {
                        zobristHash ^= zobristCastling[castlingFlag];
                        castlingFlag &= ~shortCastleFlags[!whiteTurn];
                        zobristHash ^= zobristCastling[castlingFlag];
                    } else if (capturedSquare == longRookSquares[!whiteTurn]) {
                        zobristHash ^= zobristCastling[castlingFlag];
                        castlingFlag &= ~longCastleFlags[!whiteTurn];
                        zobristHash ^= zobristCastling[castlingFlag];
                    }
                }

                clockResetIndex = previousHashes.size();

                phase = getGamePhase(whitePieces, blackPieces);
            }

            if (piece.type == KING) {
                if (whiteTurn) {
                    whiteKingSquare = endSquare;
                } else {
                    blackKingSquare = endSquare;
                }

                // For castle, the king movement is already provided in the move
                // So we only have to move the rook
                if (move.moveType == SHORT_ROQUE || move.moveType == LONG_ROQUE) {
                    Square rookStart;
                    Square rookEnd;
                    
                    if (move.moveType == SHORT_ROQUE) {
                        // Short castle
                        rookStart = shortRookSquares[whiteTurn];
                        rookEnd = shortCastleRookDestination[whiteTurn];
                    } else {
                        // Long castle
                        rookStart = longRookSquares[whiteTurn];
                        rookEnd = longCastleRookDestination[whiteTurn];
                    }

                    colorBB[whiteTurn][ROOK] &= ~bit(rookStart);
                    colorBB[whiteTurn][ROOK] |= bit(rookEnd);
                    pieces[rookStart] = EMPTY_PIECE;
                    pieces[rookEnd] = {ROOK, whiteTurn};

                    removePieceSquareScore(whiteTurn, ROOK, rookStart);
                    addPieceSquareScore(whiteTurn, ROOK, rookEnd);
                    zobristHash ^= zobristPiecesSquaresColor[whiteTurn][ROOK][rookStart];
                    zobristHash ^= zobristPiecesSquaresColor[whiteTurn][ROOK][rookEnd];

                    clockResetIndex = previousHashes.size();
                }

                // Can no longer castle after a king move
                zobristHash ^= zobristCastling[castlingFlag];
                castlingFlag &= ~CASTLE_COLOR[whiteTurn];
                zobristHash ^= zobristCastling[castlingFlag];
            } else if (piece.type == ROOK) {
                // Can't castle with a rook that has moved
                if (startSquare == shortRookSquares[whiteTurn]) {
                    zobristHash ^= zobristCastling[castlingFlag];
                    castlingFlag &= ~shortCastleFlags[whiteTurn];
                    zobristHash ^= zobristCastling[castlingFlag];
                } else if (startSquare == longRookSquares[whiteTurn]) {
                    zobristHash ^= zobristCastling[castlingFlag];
                    castlingFlag &= ~longCastleFlags[whiteTurn];
                    zobristHash ^= zobristCastling[castlingFlag];
                }
            } else if (piece.type == PAWN) {
                clockResetIndex = previousHashes.size();

                int distY = squareY(endSquare)-squareY(startSquare);

                // Check for double moves
                if (whiteTurn) {
                    if (distY == -2) {
                        enPassantBB = startBit << 8ULL;
                        zobristHash ^= zobristEnPassantFiles[squareX(endSquare)];
                    }
                } else {
                    if (distY == 2) {
                        enPassantBB = startBit >> 8ULL;
                        zobristHash ^= zobristEnPassantFiles[squareX(endSquare)];
                    }
                }
            }

            // In case of promotion
            if (move.promotionType != EMPTY) {
                pieces[endSquare] = {move.promotionType, whiteTurn};

                colorBB[whiteTurn][move.promotionType] |= endBit;
                if (whiteTurn) {
                    whitePieces[PAWN] -= 1;
                    whitePieces[move.promotionType] += 1;
                } else {
                    blackPieces[PAWN] -= 1;
                    blackPieces[move.promotionType] += 1;
                }

                addPieceSquareScore(whiteTurn, move.promotionType, endSquare);
                zobristHash ^= zobristPiecesSquaresColor[whiteTurn][move.promotionType][endSquare];
            } else {
                pieces[endSquare] = piece;

                colorBB[whiteTurn][piece.type] |= endBit;

                addPieceSquareScore(whiteTurn, piece.type, endSquare);
                zobristHash ^= zobristPiecesSquaresColor[whiteTurn][piece.type][endSquare];
            }
            removePieceSquareScore(whiteTurn, piece.type, startSquare);
            zobristHash ^= zobristPiecesSquaresColor[whiteTurn][piece.type][startSquare];

            // Clears start square
            colorBB[whiteTurn][piece.type] &= ~startBit;
            pieces[startSquare] = EMPTY_PIECE;

            // The side to move always alternates
            zobristHash ^= zobristWhiteMoves;
            whiteTurn = !whiteTurn;

            previousHashes.push_back(zobristHash);

            /*areMovesComputed = false;
            allMoves.clear();*/
            isWhiteCheckComputed = false;
            isBlackCheckComputed = false;
            calcOccupancy();

            if (!hasLegalMove()) {
                if (isInCheck(whiteTurn)) {
                    if (whiteTurn) {
                        state = BLACK_WON;
                    } else {
                        state = WHITE_WON;
                    }
                } else {
                    state = DRAW;
                }
            } else if (previousHashes.size()-clockResetIndex >= FIFTY_MOVE_RULE_PLIES) {
                // Fifty moves rule
                state = DRAW;
            } else {
                // Repetitin rule
                int repetitionCount = 0;
                for (int i = previousHashes.size()-3 ; i >= clockResetIndex ; i--) {
                    if (previousHashes[i] == zobristHash) {
                        repetitionCount += 1;

                        if (repetitionCount >= 2) {
                            state = DRAW;
                            break;
                        }
                    }
                }
            }

            return info;
        }

        void undoMove(const Move &move, const UnmakeMoveInfo &info) {
            Square startSquare = move.startSquare;
            Square endSquare = move.endSquare;

            Piece piece = getAt(endSquare);
            Piece destPiece = info.capturedPiece;

            uint64_t startBit = bit(startSquare);
            uint64_t endBit = bit(endSquare);

            castlingFlag = info.castlingFlag;
            zobristHash = info.previousZobristHash;
            pieceSquareScoreOpening = info.previousPieceSquareScoreOpening;
            pieceSquareScoreEndgame = info.previousPieceSquareScoreEndgame;
            clockResetIndex = info.previousClockResetIndex;

            previousHashes.pop_back();

            whiteTurn = !whiteTurn;

            if (enPassantBB) {
                Square enPassantSquare = bitToSquare(enPassantBB);
                enPassantBB = 0ULL;
            }
            if (info.previousEnPassantBB) {
                enPassantBB = info.previousEnPassantBB;

                Square enPassantSquare = bitToSquare(enPassantBB);
            }

            Square capturedSquare = endSquare;
            uint64_t capturedBit = endBit;
            if (move.moveType == EN_PASSANT) {
                if (whiteTurn) {
                    capturedSquare += 8;
                    capturedBit >>= 8ULL;
                } else {
                    capturedSquare -= 8;
                    capturedBit <<= 8ULL;
                }
                pieces[endSquare] = EMPTY_PIECE;
            } 
            pieces[capturedSquare] = destPiece;

            // Restores captured piece
            if (destPiece.type != EMPTY) {
                colorBB[!whiteTurn][destPiece.type] |= capturedBit;
                if (whiteTurn) {
                    blackPieces[destPiece.type] += 1;
                } else {
                    whitePieces[destPiece.type] += 1;
                }

                phase = getGamePhase(whitePieces, blackPieces);
            }

            if (piece.type == KING) {
                if (whiteTurn) {
                    whiteKingSquare = startSquare;
                } else {
                    blackKingSquare = startSquare;
                }

                // For castle, the king movement is already provided in the move
                // So we only have to move the rook
                if (move.moveType == SHORT_ROQUE || move.moveType == LONG_ROQUE) {
                    Square rookStart;
                    Square rookEnd;
                    
                    if (move.moveType == SHORT_ROQUE) {
                        // Short castle
                        rookStart = shortRookSquares[whiteTurn];
                        rookEnd = shortCastleRookDestination[whiteTurn];
                    } else {
                        // Long castle
                        rookStart = longRookSquares[whiteTurn];
                        rookEnd = longCastleRookDestination[whiteTurn];
                    }

                    colorBB[whiteTurn][ROOK] &= ~bit(rookEnd);
                    colorBB[whiteTurn][ROOK] |= bit(rookStart);
                    pieces[rookEnd] = EMPTY_PIECE;
                    pieces[rookStart] = {ROOK, whiteTurn};
                }
            }

            // In case of promotion
            if (move.promotionType != EMPTY) {
                colorBB[whiteTurn][PAWN] |= startBit;
                colorBB[whiteTurn][piece.type] &= ~endBit;
                if (whiteTurn) {
                    whitePieces[PAWN] += 1;
                    whitePieces[move.promotionType] -= 1;
                } else {
                    blackPieces[PAWN] += 1;
                    blackPieces[move.promotionType] -= 1;
                }

                pieces[startSquare] = {PAWN, whiteTurn};
            } else {
                colorBB[whiteTurn][piece.type] |= startBit;
                colorBB[whiteTurn][piece.type] &= ~endBit;

                pieces[startSquare] = piece;
            }

            /*areMovesComputed = false;
            allMoves.clear();*/
            isWhiteCheckComputed = false;
            isBlackCheckComputed = false;
            calcOccupancy();

            state = info.previousState;
        }

        // For Null Move Pruning, be caefull as it erases allMoves cache
        // TODO : Maybe should reset enPassantBB ?
        void playNullMove() {
            // Updates state
            whiteTurn = !whiteTurn;

            zobristHash ^= zobristWhiteMoves;

            /*areMovesComputed = false;
            allMoves.clear();*/
        }
        void undoNullMove() {
            // Reverts state
            whiteTurn = !whiteTurn;

            zobristHash ^= zobristWhiteMoves;

            /*areMovesComputed = false;
            allMoves.clear();*/
        }

        // All attacks on empty squares and opponent's pieces
        uint64_t capturesMask(Square square, PieceType pieceType, bool isWhite) const {
            uint64_t notSameColorPieces = ~occupencies[isWhite];

            return attacksMask(square, pieceType, isWhite) & notSameColorPieces;
        }
        // Same as captures but includes the defense of your own pieces
        uint64_t attacksMask(Square square, PieceType pieceType, bool isWhite) const {
            switch (pieceType) {
                case PAWN:
                    return colorsPawnAttacks[isWhite][square];
                case KNIGHT:
                    return knightAttacks[square];
                case KING:
                    return kingAttacks[square];

                int index;

                case BISHOP:
                    {
                        const MagicEntry bishopMagicEntry = bishopMagicEntries[square];
                        index = applyMagic(
                            bishopMagicEntry.mask & allOccupancy, 
                            bishopMagicEntry.magic, 
                            bishopMagicEntry.bitSize
                        );

                        return bishopMagicEntry.tablePtr[index];
                    }

                case ROOK:
                    {
                        const MagicEntry rookMagicEntry = rookMagicEntries[square];
                        index = applyMagic(
                            rookMagicEntry.mask & allOccupancy,
                            rookMagicEntry.magic, 
                            rookMagicEntry.bitSize
                        );

                        return rookMagicEntry.tablePtr[index];
                    }

                case QUEEN:
                    {
                        const MagicEntry diagonalsMagicEntry = bishopMagicEntries[square];
                        index = applyMagic(
                            diagonalsMagicEntry.mask & allOccupancy, 
                            diagonalsMagicEntry.magic, 
                            diagonalsMagicEntry.bitSize
                        );

                        uint64_t mask = diagonalsMagicEntry.tablePtr[index];

                        const MagicEntry straightMagicEntry = rookMagicEntries[square];
                        index = applyMagic(
                            straightMagicEntry.mask & allOccupancy, 
                            straightMagicEntry.magic, 
                            straightMagicEntry.bitSize
                        );

                        mask |= straightMagicEntry.tablePtr[index];

                        return mask;
                    }
            }

            return 0;
        }

        bool operator==(const Board &other) const {
            // Collisions are rare but not impossible ! (1/2^64 = 0,0000000000000000054 %)
            return zobristHash == other.zobristHash;
        }

        // Removes all the hashes that aren't usefull anymore
        void cleanPreviousHashes() {
            previousHashes.erase(previousHashes.begin(), previousHashes.begin() + clockResetIndex);
            clockResetIndex = 0;
        }

    private:
        /*bool areMovesComputed = false;
        std::vector<Move> allMoves = {};*/
        
        bool isWhiteCheckComputed = false;
        bool isWhiteInCheck = false;
        bool isBlackCheckComputed = false;
        bool isBlackInCheck = false;

        void calcOccupancy() {
            occupencies[0] = colorBB[BLACK][PAWN] |
                             colorBB[BLACK][BISHOP] |
                             colorBB[BLACK][KNIGHT] |
                             colorBB[BLACK][ROOK] |
                             colorBB[BLACK][QUEEN] |
                             colorBB[BLACK][KING];
            
            occupencies[1] = colorBB[WHITE][PAWN] |
                             colorBB[WHITE][BISHOP] |
                             colorBB[WHITE][KNIGHT] |
                             colorBB[WHITE][ROOK] |
                             colorBB[WHITE][QUEEN] |
                             colorBB[WHITE][KING];
            
            allOccupancy = occupencies[0] | occupencies[1];
        }

        void addPieceSquareScore(bool isWhite, PieceType pieceType, Square square) {
            pieceSquareScoreOpening += pieceValuesPosOpeningColor[isWhite][pieceType][square];
            pieceSquareScoreEndgame += pieceValuesPosEndgameColor[isWhite][pieceType][square];
        }
        void removePieceSquareScore(bool isWhite, PieceType pieceType, Square square) {
            pieceSquareScoreOpening -= pieceValuesPosOpeningColor[isWhite][pieceType][square];
            pieceSquareScoreEndgame -= pieceValuesPosEndgameColor[isWhite][pieceType][square];
        }

        // Should only be called when copying a board, use incremental updates instead
        void recomputePieceSquareScores() {
            pieceSquareScoreOpening =
                piecesValue(PAWN,   BLACK, colorBB[BLACK][PAWN],   pieceValuesPosOpeningColor) +
                piecesValue(BISHOP, BLACK, colorBB[BLACK][BISHOP], pieceValuesPosOpeningColor) +
                piecesValue(KNIGHT, BLACK, colorBB[BLACK][KNIGHT], pieceValuesPosOpeningColor) +
                piecesValue(ROOK,   BLACK, colorBB[BLACK][ROOK],   pieceValuesPosOpeningColor) +
                piecesValue(QUEEN,  BLACK, colorBB[BLACK][QUEEN],  pieceValuesPosOpeningColor) +
                pieceValuesPosOpeningColor[BLACK][KING][blackKingSquare] +

                piecesValue(PAWN,   WHITE, colorBB[WHITE][PAWN],   pieceValuesPosOpeningColor) + 
                piecesValue(BISHOP, WHITE, colorBB[WHITE][BISHOP], pieceValuesPosOpeningColor) + 
                piecesValue(KNIGHT, WHITE, colorBB[WHITE][KNIGHT], pieceValuesPosOpeningColor) + 
                piecesValue(ROOK,   WHITE, colorBB[WHITE][ROOK],   pieceValuesPosOpeningColor) + 
                piecesValue(QUEEN,  WHITE, colorBB[WHITE][QUEEN],  pieceValuesPosOpeningColor) + 
                pieceValuesPosOpeningColor[WHITE][KING][whiteKingSquare];
            
            pieceSquareScoreEndgame =
                piecesValue(PAWN,   BLACK, colorBB[BLACK][PAWN],   pieceValuesPosEndgameColor) +
                piecesValue(BISHOP, BLACK, colorBB[BLACK][BISHOP], pieceValuesPosEndgameColor) +
                piecesValue(KNIGHT, BLACK, colorBB[BLACK][KNIGHT], pieceValuesPosEndgameColor) +
                piecesValue(ROOK,   BLACK, colorBB[BLACK][ROOK],   pieceValuesPosEndgameColor) +
                piecesValue(QUEEN,  BLACK, colorBB[BLACK][QUEEN],  pieceValuesPosEndgameColor) +
                pieceValuesPosEndgameColor[BLACK][KING][blackKingSquare] +

                piecesValue(PAWN,   WHITE, colorBB[WHITE][PAWN],   pieceValuesPosEndgameColor) + 
                piecesValue(BISHOP, WHITE, colorBB[WHITE][BISHOP], pieceValuesPosEndgameColor) + 
                piecesValue(KNIGHT, WHITE, colorBB[WHITE][KNIGHT], pieceValuesPosEndgameColor) + 
                piecesValue(ROOK,   WHITE, colorBB[WHITE][ROOK],   pieceValuesPosEndgameColor) + 
                piecesValue(QUEEN,  WHITE, colorBB[WHITE][QUEEN],  pieceValuesPosEndgameColor) + 
                pieceValuesPosEndgameColor[WHITE][KING][whiteKingSquare];
        }
};


Board loadBoard(uint64_t colorBitBoards[2][PIECE_TYPE_COUNT],
                char castlingFlag,
                bool whiteTurn) {
    uint64_t zobristHash = zobristWhiteMoves * whiteTurn;

    zobristHash ^= zobristCastling[castlingFlag];

    Piece pieces[64];
    int whitePieces[PIECE_TYPE_COUNT] = {};
    int blackPieces[PIECE_TYPE_COUNT] = {};

    Square whiteKingSquare;
    Square blackKingSquare;

    for (int y = 0 ; y < ROW_COUNT ; y++) {
        for (int x = 0 ; x < ROW_COUNT ; x++) {
            uint64_t b = bit(x, y);
            Square square = x + 8u*y;

            Piece piece;
            if (b & colorBitBoards[BLACK][PAWN]) { piece = {PAWN, BLACK}; }
            else if (b & colorBitBoards[BLACK][BISHOP]) { piece = {BISHOP, BLACK}; }
            else if (b & colorBitBoards[BLACK][KNIGHT]) { piece = {KNIGHT, BLACK}; } 
            else if (b & colorBitBoards[BLACK][ROOK]) { piece = {ROOK, BLACK}; } 
            else if (b & colorBitBoards[BLACK][QUEEN]) { piece = {QUEEN, BLACK}; } 
            else if (b & colorBitBoards[BLACK][KING]) { piece = {KING, BLACK}; blackKingSquare = square; } 
            else if (b & colorBitBoards[WHITE][PAWN]) { piece = {PAWN, WHITE}; }
            else if (b & colorBitBoards[WHITE][BISHOP]) { piece = {BISHOP, WHITE}; }
            else if (b & colorBitBoards[WHITE][KNIGHT]) { piece = {KNIGHT, WHITE}; } 
            else if (b & colorBitBoards[WHITE][ROOK]) { piece = {ROOK, WHITE}; } 
            else if (b & colorBitBoards[WHITE][QUEEN]) { piece = {QUEEN, WHITE}; } 
            else if (b & colorBitBoards[WHITE][KING]) { piece = {KING, WHITE}; whiteKingSquare = square; } 
            else { piece = EMPTY_PIECE; }

            pieces[square] = piece;

            if (piece.type != EMPTY) {
                if (piece.isWhite) {
                    whitePieces[piece.type] += 1;
                } else {
                    blackPieces[piece.type] += 1;
                }

                zobristHash ^= zobristPiecesSquaresColor[piece.isWhite][piece.type][square];
            }
        }
    }

    GamePhase phase = getGamePhase(whitePieces, blackPieces);

    // Assumes the game is not finished, it is the opening and the half move clock is zero
    return Board(whiteTurn, colorBitBoards, 0ULL, whiteKingSquare, blackKingSquare, NEUTRAL, phase, whitePieces, blackPieces, pieces, castlingFlag, /*false, 0.0f,*/ zobristHash, {}, 0);
}
Board loadBoardFile(std::ifstream &file, int boardIndex) {
    // In bytes
    constexpr int storedBoardSize = 98;

    uint64_t colorBitBoards[2][PIECE_TYPE_COUNT] = {};
    char castlingFlag;
    bool whiteTurn;

    file.seekg(boardIndex * storedBoardSize);
    file.read((char *) colorBitBoards, 2 * PIECE_TYPE_COUNT * sizeof(uint64_t));
    file.read((char *) &castlingFlag, 1);
    file.read((char *) &whiteTurn, 1);

    return loadBoard(colorBitBoards, castlingFlag, whiteTurn);
}
// For debug purposes
void printBoard(const Board &board) {
    std::string line = {};

    for (int y = 0 ; y < ROW_COUNT ; y++) {
        line.clear();
        for (int x = 0 ; x < ROW_COUNT ; x++) {
            Piece piece = board.getAt(x, y);

            if (piece.type == EMPTY) {
                line.append("□ ");
            } else {
                line.append(piecesEmojiColor[piece.isWhite][piece.type]);
                line.append(" ");
            }
        }
        printf(line.c_str());
        printf("\n");
    }
}


#endif
