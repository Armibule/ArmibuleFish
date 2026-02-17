#define SDL_MAIN_HANDLED

#include <iostream>
#include <string>
#include "engine/bot.cpp"


/*
    uci, outputs id responses + uciok
    isready, outputs readyok
    ucinewgame
    position
    go, outputs bestmove
    stop
    quit
*/


struct UCIMove {
    Square startSquare;
    Square endSquare;

    PieceType promotionType = EMPTY;
};


// Only do on the position preceding the move
Move convertMove(const Board &board, const UCIMove &uciMove) {
    Square startSquare = uciMove.startSquare;
    Square endSquare = uciMove.endSquare;

    Piece piece = board.getAt(startSquare);
    Move move = {startSquare, endSquare, uciMove.promotionType};

    if (piece.type == KING) {
        if ( (piece.isWhite && (startSquare == makeSquare(4, 7))) ||
             (!piece.isWhite && (startSquare == makeSquare(4, 0))) ) {

            if (endSquare == shortCastleKingDestination[piece.isWhite]) {
                move.moveType = SHORT_CASTLE;
            } else if (endSquare == longCastleKingDestination[piece.isWhite]) {
                move.moveType = LONG_CASTLE;
            }
        }
    } else if (piece.type == PAWN) {
        if (squareX(startSquare) != squareX(endSquare) && board.getAt(endSquare).type == EMPTY) {
            move.moveType = EN_PASSANT;
        } 
    }

    return move;
}


PieceType uciPromotionType(char l) {
    switch (l) {
        case 'q':
            return QUEEN;
        case 'r':
            return ROOK;
        case 'b':
            return BISHOP;
        case 'n': 
            return KNIGHT;
    }
    return EMPTY;
}
char promotionTypeToUCI(PieceType pieceType) {
    switch (pieceType) {
        case QUEEN:
            return 'q';
        case ROOK:
            return 'r';
        case BISHOP:
            return 'b';
        case KNIGHT: 
            return 'n';
    };
    return ' ';
}

// Can't determine the move type -> convert uci move to normal move later
// Doesn't handle null moves "0000"
UCIMove moveFromUCI(std::string uci) {
    UCIMove uciMove = {
        fenCoordinateToSquare(uci[0], uci[1]),
        fenCoordinateToSquare(uci[2], uci[3]),
    };

    if (uci.size() > 4) {
        uciMove.promotionType = uciPromotionType(uci[4]);
    }
    return uciMove;
}
std::vector<UCIMove> movesFromUCI(std::string uci) {
    std::vector<UCIMove> uciMoves = {};
    std::string acc = "";

    for (int i = 0 ; i < uci.size() ; i++) {
        char l = uci[i];

        if (l == ' ') {
            uciMoves.push_back(moveFromUCI(acc));
            acc = "";
        } else {
            acc.push_back(l);
        }
    }
    if (acc.size() > 0) {
        uciMoves.push_back(moveFromUCI(acc));
    }
    return uciMoves;
}
// Plays the sequence of UCI moves to a board
void applyUCIMovesToBoard(Board &board, std::vector<UCIMove> uciMoves) {
    for (UCIMove uciMove : uciMoves) {
        Move move = convertMove(board, uciMove);
        board.playMove(move);
    }
}
std::string moveToUCI(const Move &move) {
    std::string uci = "....";

    FenCoordinateChars startChars = squareToFenCoordinate(move.startSquare);
    FenCoordinateChars endChars = squareToFenCoordinate(move.endSquare);

    uci[0] = startChars.letter;
    uci[1] = startChars.digit;
    uci[2] = endChars.letter;
    uci[3] = endChars.digit;

    if (move.promotionType != EMPTY) {
        uci.push_back(promotionTypeToUCI(move.promotionType));
    }
    return uci;
}
// Use space separation
std::vector<std::string> splitCommand(std::string line) {
    std::string value = "";
    std::vector<std::string> values = {};

    for (int i = 0 ; i < line.size() ; i++) {
        char c = line[i];
        if (c == ' ') {
            values.push_back(value);
            value = "";
        } else {
            value.push_back(c);
        }
    }
    if (value.size() > 0) {
        values.push_back(value);
    }
    return values;
}

Bot * bot;
Move ponderMove = NO_MOVE;
Board board = {};

int givenTime = 0;
int whiteTime = 0;
int blackTime = 0;
int whiteInc = 0;
int blackInc = 0;

std::thread * ponderThread = nullptr;
MoveResult ponderResult;
// Should be run in a thread
void ponder(Board boardCopy) {
    // Starts thinking for 100 seconds
    DEFAULT_BOT_TIME = 100'000.0f;
    MAX_BOT_TIME = 100'000.0f;

    // The move should have already been provided by the gui at this point
    ponderResult = bot->getBestMove(boardCopy, false, false, true);
}
// Don't really know if it is thread safe
void ensurePonderingStopped() {
    bot->stopSearch();

    if (ponderThread) {
        if (ponderThread->joinable()) {
            ponderThread->join();
        }
    }
}


void setBotTime() {
    if (givenTime == 0) {
        if (board.whiteTurn) {
            DEFAULT_BOT_TIME = ((float) whiteTime) / 18.0f + ((float) whiteInc) / 2.0f;
        } else {
            DEFAULT_BOT_TIME = ((float) blackTime) / 18.0f + ((float) blackInc) / 2.0f;
        }
    } else {
        DEFAULT_BOT_TIME = (float) givenTime;
    }

    MAX_BOT_TIME = DEFAULT_BOT_TIME * 4.0f;

    std::vector<Move> possibleMoves = {};
    board.getAllMoves(possibleMoves);
    if (possibleMoves.size() == 1) {
        // Plays as fast as possible
        DEFAULT_BOT_TIME = 1.0f;
        MAX_BOT_TIME = 100.0f;
    }
}


int main(int argc, char * argv[]) {
    genBitboardConstants();
    genZobristKeys();
    initBot();

    bot = new Bot();
    
    // std::ofstream outputTestFile {"outputTest.txt", std::ofstream::binary};

    bool running = true;
    std::string command;
    while ( running ) {

        command = "";
        std::getline(std::cin, command, '\n');
        // outputTestFile << command << std::endl;

        if (command == "uci") {
            std::cout << "id name Armibule Fish\n";
            std::cout << "id author Armibule\n";
            std::cout << "uciok" << std::endl;
        } else if (command == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (command == "ucinewgame") {
            ensurePonderingStopped();
            bot->resetBot();
        } else if (command.starts_with("position")) {
            ensurePonderingStopped();
            if (command.substr(9, 3) == "fen") {
                size_t movesPosition = command.find("moves");

                if (movesPosition == std::string::npos) {
                    // No moves provided
                    std::string fenString = command.substr(13,command.length() - 13);
                    board = loadFEN(fenString);
                } else {
                    std::string fenString = command.substr(13,movesPosition - 13);
                    board = loadFEN(fenString);

                    int movesIndex = movesPosition + 6;
                    
                    std::string uciMovesString = command.substr(movesIndex, command.length() - movesIndex);
                    std::vector<UCIMove> uciMoves = movesFromUCI(uciMovesString);
                    applyUCIMovesToBoard(board, uciMoves);
                    board.cleanPreviousHashes();
                    while (bot->halfMoveTick < uciMoves.size()) {
                        bot->onMovePlayed(board);
                    }
                }

            } else if (command.substr(9, 8) == "startpos") {
                board = {};

                size_t movesPosition = command.find("moves");

                if (movesPosition != std::string::npos) {
                    // Moves were provided
                    int movesIndex = movesPosition + 6;
                    
                    std::string uciMovesString = command.substr(movesIndex, command.length() - movesIndex);
                    std::vector<UCIMove> uciMoves = movesFromUCI(uciMovesString);
                    applyUCIMovesToBoard(board, uciMoves);
                    while (bot->halfMoveTick < uciMoves.size()) {
                        bot->onMovePlayed(board);
                    }
                }
            } else {
                // otherwise, skip command 
                continue;
            }
        } else if (command.starts_with("stop")) {

            ensurePonderingStopped();
            std::cout << "bestmove " << moveToUCI(ponderResult.move) << std::endl;

        } else if (command.starts_with("go ponder")) {

            ensurePonderingStopped();
            if (ponderThread != nullptr) {
                free(ponderThread);
                ponderThread = nullptr;
            }

            std::vector<std::string> splitted = splitCommand(command);

            givenTime = 0;
            for (int i = 2 ; i < splitted.size() ; i++) {
                if (splitted[i] == "movetime") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    givenTime = atoi(splitted[i].c_str());
                } else if (splitted[i] == "wtime") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    whiteTime = atoi(splitted[i].c_str());
                } else if (splitted[i] == "btime") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    blackTime = atoi(splitted[i].c_str());
                } else if (splitted[i] == "winc") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    whiteInc = atoi(splitted[i].c_str());
                }
                 else if (splitted[i] == "binc") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    blackInc = atoi(splitted[i].c_str());
                }
            }

            ponderThread = new std::thread(ponder, board.copy());

        }  else if (command.starts_with("ponderhit")) {

            ensurePonderingStopped();

            setBotTime();
            MoveResult bestResult = bot->getBestMove(board, false, false, true);
            std::string uciMoveString = moveToUCI(bestResult.move);

            board.playMove(bestResult.move);
            bot->onMovePlayed(board);

            ponderMove = bot->principalVariation[0]; 

            if (ponderMove == NO_MOVE) {
                std::cout << "bestmove " << uciMoveString << " ponder 0000" << std::endl;
            } else {
                std::cout << "bestmove " << uciMoveString << " ponder " << moveToUCI(ponderMove) << std::endl;
            }

        } else if (command.starts_with("go")) {

            ensurePonderingStopped();
            
            std::vector<std::string> splitted = splitCommand(command);

            givenTime = 0;
            for (int i = 1 ; i < splitted.size() ; i++) {
                if (splitted[i] == "movetime") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    givenTime = atoi(splitted[i].c_str());
                } else if (splitted[i] == "wtime") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    whiteTime = atoi(splitted[i].c_str());
                } else if (splitted[i] == "btime") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    blackTime = atoi(splitted[i].c_str());
                } else if (splitted[i] == "winc") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    whiteInc = atoi(splitted[i].c_str());
                }
                 else if (splitted[i] == "binc") {
                    i += 1;
                    if (i > splitted.size()) { break; }
                    blackInc = atoi(splitted[i].c_str());
                }
            }

            setBotTime();

            MoveResult bestResult = bot->getBestMove(board, false, false, true);
            std::string uciMoveString = moveToUCI(bestResult.move);

            board.playMove(bestResult.move);
            bot->onMovePlayed(board);

            ponderMove = bot->principalVariation[0]; 

            if (ponderMove == NO_MOVE) {
                std::cout << "bestmove " << uciMoveString << " ponder 0000" << std::endl;
            } else {
                std::cout << "bestmove " << uciMoveString << " ponder " << moveToUCI(ponderMove) << std::endl;
            }
        } else if (command == "quit") {
            running = false;
        }
        
    };

    ensurePonderingStopped();
    return 0;
}


