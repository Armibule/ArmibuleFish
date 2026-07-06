#ifndef GAME
#define GAME


#include <SDL2/SDL2_gfxPrimitives.h>
#include "engine/bot.cpp"
#include "shared.cpp"
#include <array>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>



const int MOVE_CLASSIFICATION_COUNT = 9;

enum MoveClassification : int {
    MOVE_BRILLIANT = 0,
    MOVE_GREAT = 1,
    MOVE_BEST = 2,
    MOVE_EXCELLENT = 3,
    MOVE_OK = 4,
    MOVE_INACCURACY = 5,
    MOVE_MISTAKE = 6,
    MOVE_MISSED = 7,
    MOVE_BLUNDER = 8,
    NO_CLASSIFICATION = -1
};


class Game {
    public:
        Board board;
        Shared * shared;
        Bot * bot;

        // Set to NULL when no search is being made
        std::mutex botThreadMutex;
        std::thread * botThread = NULL;
        MoveResult obtainedResult = {};     // Check if move is NO_MOVE to see if there is a result to process

        std::vector<Move> predictedVariation = {};

        float piecesDisplayPosX[64] = {};
        float piecesDisplayPosY[64] = {};
        float pieceDisplayPosLerp = 0.175f;

        Game() = delete;
        Game(SDL_Renderer* renderer, Shared * shared, Bot * bot, Board &board) : renderer(renderer)  {
            this->shared = shared;
            this->bot = bot;

            this->board = board;
            
            boardRect = {(int) (SCREEN_WIDTH - SCREEN_HEIGHT * 0.8)/2, (int) (SCREEN_HEIGHT * 0.1), (int) (SCREEN_HEIGHT * 0.8), (int) (SCREEN_HEIGHT * 0.8)};
            boardOutline1 = {boardRect.x - 8, boardRect.y - 8, boardRect.w + 16, boardRect.h + 16};
            boardOutline2 = {boardRect.x - 6, boardRect.y - 6, boardRect.w + 12, boardRect.h + 12};

            evaluationBarRect = {boardRect.x - 60, boardRect.y, 30, boardRect.h};
            evaluationBarOutline = {evaluationBarRect.x - 2, evaluationBarRect.y - 2, evaluationBarRect.w + 4, evaluationBarRect.h + 4};

            cellRect = {0, 0, boardRect.w / ROW_COUNT, boardRect.w / ROW_COUNT};
            promotionRect = {boardRect.x, boardRect.y, cellRect.w / 2, cellRect.w / 2};
            
            circleRect = {boardRect.x, boardRect.y, (int) (cellRect.w * 0.6), (int) (cellRect.w * 0.6)};

            captureIconRect = {0, 0, captureIconSize, captureIconSize};

            moveClassificationRect = {0, 0, moveClassificationSize, moveClassificationSize};

            loadTextures();

            for (int i = 0 ; i < 64 ; i++) {
                piecesDisplayPosX[i] = boardRect.x + boardRect.w * 0.5f;
                piecesDisplayPosY[i] = boardRect.y + boardRect.h * 0.5f;
            }
        };

        void update() {
            int xMouse, yMouse;
            SDL_GetMouseState( &xMouse, &yMouse );

            checkBotMove();
            updateBotInfo();

            drawBoard(xMouse, yMouse);

            drawMoveClassification();
            
            if ( holdPieceMoves.size() > 0 ) {
                shared->uiActive = 40;

                drawMoves(xMouse, yMouse);
            }

            drawEvalutionBar();
            drawCaptures();

            if (shared->showPV) {
                drawPV();
            }

            if (holdPieceMoves.size() > 0) {
                shared->currentCursor = shared->CURSOR_HAND;
            }
        }

        void updatePiecesDisplayPos() {
            int screenX, screenY;
            for (Square square = 0 ; square < 64 ; square++) {
                squareToScreenPos(square, screenX, screenY);

                piecesDisplayPosX[square] += ((float) screenX - piecesDisplayPosX[square])*pieceDisplayPosLerp * shared->animationSpeed;
                piecesDisplayPosY[square] += ((float) screenY - piecesDisplayPosY[square])*pieceDisplayPosLerp * shared->animationSpeed;
            }
        }

        void drawPieces(int xMouse, int yMouse) {
            updatePiecesDisplayPos();
            
            for (Square square = 0 ; square < 64 ; square++) {
                // Can be refoctored
                if ( isPromotionAsked && (askedPromotionStartPos.x == squareX(square) || askedPromotionStartPos.y == squareY(square)) ) {
                    continue;
                }
                if ( holdPieceMoves.size() != 0 && squareX(square) == holdPiecePos.x && squareY(square) == holdPiecePos.y ) {
                    continue;
                }

                Piece piece = board.getAt(square);

                cellRect.x = std::roundl(piecesDisplayPosX[square]);
                cellRect.y = std::roundl(piecesDisplayPosY[square]);

                drawPiece(piece, cellRect);
            }
        }

        void drawBoard(int xMouse, int yMouse) {
            SDL_SetRenderDrawColor( renderer, 255, 206, 158, 255 );
            SDL_RenderFillRect( renderer, &boardOutline1 );
            
            if (board.whiteTurn) {
                SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );
                SDL_RenderFillRect( renderer, &boardOutline2 );
            } else {
                SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
                SDL_RenderFillRect( renderer, &boardOutline2 );
            }

            // Draw checkerboard pattern
            for (char y = 0 ; y < ROW_COUNT ; y++) {
                for (char x = 0 ; x < ROW_COUNT ; x++) {
                    moveRectToBoardPos(cellRect, x, y);

                    if ((x + y) % 2 == 0) {
                        SDL_SetRenderDrawColor( renderer, 255, 206, 158, 255 );
                    } else {
                        SDL_SetRenderDrawColor( renderer, 209, 139, 71, 255 );
                    }
                    SDL_RenderFillRect( renderer, &cellRect );
                }
            }

            // Draw highlited squares
            for (BoardPos &pos : highlitedSquares) {
                moveRectToBoardPos(cellRect, pos.x, pos.y);

                if ((pos.x + pos.y) % 2 == 0) {
                    SDL_SetRenderDrawColor( renderer, 238, 131, 131, 255 );
                } else {
                    SDL_SetRenderDrawColor( renderer, 223, 88, 88, 255 );
                }

                SDL_RenderFillRect( renderer, &cellRect );
            }

            // Draw pieces
            drawPieces(xMouse, yMouse);

            // Draw promotion selector
            if (isPromotionAsked) {
                moveRectToBoardPos(cellRect, askedPromotionEndPos.x, askedPromotionEndPos.y);
                std::array<SDL_Rect, PROMOTION_PIECES_COUNT> promotionRects = getPromotionRects(askedPromotionEndPos);

                for (int i = 0 ; i < PROMOTION_PIECES_COUNT ; i++) {
                    promotionRect = promotionRects[i];
                    drawPromotionPiece( Piece{promotionTypes[i], board.whiteTurn}, xMouse, yMouse );
                }
            }
        }

        void squareToScreenPos(Square square, int &screenX, int &screenY) {
            boardPosToScreenPos(squareX(square), squareY(square), screenX, screenY);
        }
        void boardPosToScreenPos(char boardX, char boardY, int &screenX, int &screenY) {
            screenX = boardX * cellRect.w + boardRect.x;
            screenY = boardY * cellRect.w + boardRect.y;
        }

        void moveRectToBoardPos(SDL_Rect &rect, char x, char y) {
            rect.x = x * cellRect.w + boardRect.x;
            rect.y = y * cellRect.w + boardRect.y;
        }

        // Draws available moves + the currently hold piece
        void drawMoves(int xMouse, int yMouse) {
            for (const Move &move : holdPieceMoves) {
                circleRect.x = squareX(move.endSquare) * cellRect.w + boardRect.x + (cellRect.w - circleRect.w)/2;
                circleRect.y = squareY(move.endSquare) * cellRect.w + boardRect.y + (cellRect.w - circleRect.w)/2;
                
                // render circle only if move don't contains promotion or promotes to knight to avoid repetition
                if (move.promotionType == EMPTY || move.promotionType == KNIGHT) {
                    if ( board.getAt(move.endSquare).type == EMPTY ) {
                        SDL_RenderCopy(renderer, moveCircleTexture, NULL, &circleRect);
                    } else {
                        SDL_RenderCopy(renderer, captureCircleTexture, NULL, &circleRect);
                    }
                }
            }

            cellRect.x = xMouse - cellRect.w/2;
            cellRect.y = yMouse - cellRect.w/2;
            drawPiece(board.getAt(holdPiecePos), cellRect);
        }

        void drawEvalutionBar() {
            displayBotEvaluation += shared->animationSpeed * (botEvaluation-displayBotEvaluation)/15.0f;

            SDL_SetRenderDrawColor( renderer, 128, 128, 128, 255 );
            SDL_RenderFillRect( renderer, &evaluationBarOutline );

            float whiteProgress;
            if (displayBotEvaluation < 0) {
                whiteProgress = -1.0f/(displayBotEvaluation*0.50f - 2.0f);
            } else {
                whiteProgress = 1.0f - 1.0f/(displayBotEvaluation*0.50f + 2.0f);
            }
            whiteProgress = (whiteProgress - 0.5f)*1.1f + 0.5f;
            
            SDL_Rect blackRegionRect = {evaluationBarRect.x, evaluationBarRect.y, evaluationBarRect.w, std::clamp((int) std::round(evaluationBarRect.h * (1.0f - whiteProgress)), 0, evaluationBarRect.h)};
            SDL_Rect whiteRegionRect = {blackRegionRect.x, blackRegionRect.y + blackRegionRect.h, blackRegionRect.w, evaluationBarRect.h - blackRegionRect.h};

            SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
            SDL_RenderFillRect( renderer, &blackRegionRect );

            SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );
            SDL_RenderFillRect( renderer, &whiteRegionRect );
        }

        void drawCaptures() {
            captureIconRect.x = boardRect.x;
            captureIconRect.y = boardRect.y - captureIconSize - 10;
            
            for (PieceType type : pieceTypes) {
                for (int i = 0 ; i < piecesCount[type] - board.whitePieces[type] ; i++) {
                    SDL_RenderCopy(renderer, whiteCaptureTextures[type], NULL, &captureIconRect);
                    captureIconRect.x += captureIconSize + captureIconPadding;
                }
            }

            captureIconRect.x = boardRect.x;
            captureIconRect.y = boardRect.y + boardRect.h + 10;
            
            for (PieceType type : pieceTypes) {
                for (int i = 0 ; i < piecesCount[type] - board.blackPieces[type] ; i++) {
                    SDL_RenderCopy(renderer, blackCaptureTextures[type], NULL, &captureIconRect);
                    captureIconRect.x += captureIconSize + captureIconPadding;
                }
            }
        }

        // Draws the principal variation according to the bot with arrows
        void drawPV() {
            int width = 6;
            int alpha = 240;

            for (const Move &move : predictedVariation) {
                BoardPos startPos = {(char) squareX(move.startSquare), (char) squareY(move.startSquare)};
                BoardPos endPos = {(char) squareX(move.endSquare), (char) squareY(move.endSquare)};

                drawArrow(startPos, endPos, width, alpha);
                
                width -= 1;
                alpha -= 20;

                if (width <= 1) {
                    break;
                }
            }
        }

        void drawArrow(BoardPos &startPos, BoardPos &endPos, int width, int alpha) {
            float startX = startPos.x * cellRect.w + boardRect.x + cellRect.w/2;
            float startY = startPos.y * cellRect.w + boardRect.y + cellRect.w/2;

            float endX = endPos.x * cellRect.w + boardRect.x + cellRect.w/2;
            float endY = endPos.y * cellRect.w + boardRect.y + cellRect.w/2;

            float dx = endX - startX;
            float dy = endY - startY;
            float d = std::sqrt(dx*dx + dy*dy);

            int xMouse, yMouse;
            SDL_GetMouseState( &xMouse, &yMouse );

            /*rgb(238, 234, 131)*/
            const int arrowR = 238;
            const int arrowG = 234;
            const int arrowB = 131;

            
            float widness = d*0.5f;
            float tipLength = width*4.0f;

            float xA = startX - dy*widness/d;
            float yA = startY + dx*widness/d;
            float d2 = std::sqrt((xA - endX)*(xA - endX) + (yA - endY)*(yA - endY));
            
            float xB = startX + dy*widness/d;
            float yB = startY - dx*widness/d;

            thickLineRGBA(
                renderer, startX, startY, 
                std::lround(startX + dx - (tipLength - 1.5f)*dx/d), std::lround(startY + dy - (tipLength - 1.5f)*dy/d), 
                width, arrowR, arrowG, arrowB, alpha
            );
            filledTrigonRGBA(
                renderer, 
                endX, endY, 
                std::lround(endX + (xA - endX)*tipLength/d2), std::lround(endY + (yA - endY)*tipLength/d2),
                std::lround(endX + (xB - endX)*tipLength/d2), std::lround(endY + (yB - endY)*tipLength/d2),
                arrowR, arrowG, arrowB, alpha
            );
            aatrigonRGBA(
                renderer, 
                endX, endY, 
                std::lround(endX + (xA - endX)*tipLength/d2), std::lround(endY + (yA - endY)*tipLength/d2),
                std::lround(endX + (xB - endX)*tipLength/d2), std::lround(endY + (yB - endY)*tipLength/d2),
                arrowR, arrowG, arrowB, alpha
            );
        }

        void drawMoveClassification() {
            if (currentMoveClassification != NO_CLASSIFICATION && moveHistory.size() > 0) {
                Square endSquare = moveHistory.back().endSquare;
                squareToScreenPos(endSquare, moveClassificationRect.x, moveClassificationRect.y);

                moveClassificationRect.x += cellRect.w - moveClassificationRect.w / 2;
                moveClassificationRect.y -= moveClassificationRect.h / 2;

                SDL_RenderCopy(renderer, moveClassificationTextures[currentMoveClassification], NULL, &moveClassificationRect);
            }
        }

        void mouseDown(const int mouseX, const int mouseY) {
            if (board.state != NEUTRAL || botThread != NULL) {
                return;
            }

            if (!isPromotionAsked) {
                SDL_Point point{mouseX, mouseY};
                if (SDL_PointInRect(&point, &boardRect)) {
                    BoardPos pos = posToBoard(mouseX, mouseY);
                    Piece holdPiece = board.getAt(pos);

                    if (holdPiece.type != EMPTY && holdPiece.isWhite == board.whiteTurn) {
                        holdPiecePos = pos;

                        holdPieceMoves.clear();
                        board.pieceMoves(makeSquare(pos.x, pos.y), holdPiece.type, holdPiece.isWhite, holdPieceMoves);
                    }
                }
            }
        }

        void mouseUp(const int mouseX, const int mouseY) {
            if (board.state != NEUTRAL || botThread != NULL) {
                return;
            }

            SDL_Point point {mouseX, mouseY};
            if ( SDL_PointInRect(&point, &boardRect) ) {
                BoardPos pos = posToBoard(mouseX, mouseY);

                if (isPromotionAsked) {
                    std::array<SDL_Rect, PROMOTION_PIECES_COUNT> promotionRects = getPromotionRects(pos);

                    for (int i{0} ; i < PROMOTION_PIECES_COUNT ; i++) {
                        if ( SDL_PointInRect(&point, &promotionRects[i]) ) {
                            isPromotionAsked = false;

                            Move move {makeSquare(askedPromotionStartPos), makeSquare(askedPromotionEndPos), promotionTypes[i]};
                            playMove(move);
                            
                            break;
                        }
                    }

                } else if ( holdPieceMoves.size() > 0 ) {
                    for (const Move &move : holdPieceMoves) {
                        if ( makeSquare(pos) == move.endSquare ) {

                            if (move.promotionType == EMPTY) {
                                playMove(move);
                                printf("----- Move played -----\n");
                            } else {
                                isPromotionAsked = true;
                                askedPromotionStartPos = {(char) squareX(move.startSquare), (char) squareY(move.startSquare)};
                                askedPromotionEndPos = {(char) squareX(move.endSquare), (char) squareY(move.endSquare)};
                            }

                            break;
                        }
                    }
                }
            }
            holdPieceMoves.clear();
        }

        void updateBotInfo() {
            if (botThread != NULL) {
                shared->currentCursor = shared->CURSOR_WAIT_ARROW;
                shared->update();

                updateDebugText();
                if (board.whiteTurn) {
                    setEvalValue(bot->currentResult.score);
                } else {
                    setEvalValue(-bot->currentResult.score);
                }
                predictedVariation = bot->principalVariation;
            }
        }

        void botPlays() {
            if (board.state != NEUTRAL) {
                std::cout << "----- The game is over -----\n" << std::endl;
                return;
            }
            if (botThread != NULL) {
                return;
            }

            shared->currentCursor = shared->CURSOR_WAIT_ARROW;
            shared->update();

            printf("----- Bot thinks -----\n");

            botThread = new std::thread(startBotSearch, this, bot, &board);
            
            // in case the user holds a piece at the same time
            holdPieceMoves.clear();
        }

        void checkBotMove() {
            if (botThread == NULL) {
                return;
            }

            botThreadMutex.lock();
            MoveResult bestResult = obtainedResult;
            botThreadMutex.unlock();

            if (obtainedResult.move == NO_MOVE) {
                return;
            }

            // If not the bot has played
            botThread->join();       // In case there is some thing left
            
            playMove(bestResult.move);
            setEvalValue(bestResult.score);

            predictedVariation = bot->principalVariation;

            // FOR DEBUG
            printf("| Principal variation:\n");
            for (const Move &move : bot->principalVariation) {
                printMove(move);
            }

            if (board.state == WHITE_WON) {
                printf("| White Won !\n");
            } else if (board.state == BLACK_WON) {
                printf("| Black Won !\n");
            } else if (board.state == DRAW) {
                printf("| It is a draw\n");
            }

            // FOR DEBUG
            bot->printTTInfos();
            bot->printMoveHistoryInfos();
            // bot->printCorrHistInfo();
            std::cout.flush();

            updateDebugText();

            // Erases current result for future ones
            botThread->~thread();
            botThread = NULL;
            obtainedResult.move = NO_MOVE;

            shared->uiActive = 40;
        }

        void restorePredictedMoves() {
            TTEntry ttEntry = bot->transpositionTable[bot->getTTIndex(board)];
            if (ttEntry.zobristHash == board.zobristHash) {
                // Score is probably a bound rather than an exact score
                if (board.whiteTurn) {
                    setEvalValue(ttEntry.score);
                } else {
                    setEvalValue(-ttEntry.score);
                }

                Board boardCopy = board.copy();

                int depthCounter = 12;   // Maximum depth

                Move pvMove = ttEntry.bestMove;
                while (pvMove != NO_MOVE && ttEntry.zobristHash == boardCopy.zobristHash && depthCounter > 0) {
                    predictedVariation.push_back(pvMove);
                
                    boardCopy.playMove(pvMove);
                
                    ttEntry = bot->transpositionTable[bot->getTTIndex(boardCopy)];
                    pvMove = ttEntry.bestMove;

                    depthCounter -= 1;
                }
            }
        }

        void playMove(const Move &move) {
            Square startSquare = move.startSquare;
            Square endSquare = move.endSquare;

            highlitedSquares.clear();

            highlitedSquares.push_back({(char) squareX(startSquare), (char) squareY(startSquare)});
            highlitedSquares.push_back({(char) squareX(endSquare), (char) squareY(endSquare)});

            moveHistory.push_back(move);
            unmakeInfos.push_back(board.playMove(move));
            bot->onMovePlayed(board);

            // Adjust position of moved piece to animate it
            if ( holdPieceMoves.size() > 0  && squareX(startSquare) == holdPiecePos.x && squareY(startSquare) == holdPiecePos.y) {
                int xMouse, yMouse;
                SDL_GetMouseState( &xMouse, &yMouse );
                piecesDisplayPosX[endSquare] = (float) (xMouse - cellRect.w/2);
                piecesDisplayPosY[endSquare] = (float) (yMouse - cellRect.w/2);
            } else {
                int screenX, screenY;
                boardPosToScreenPos(squareX(startSquare), squareY(startSquare), screenX, screenY);
                piecesDisplayPosX[endSquare] = (float) screenX;
                piecesDisplayPosY[endSquare] = (float) screenY;
            }

            predictedVariation.clear();

            restorePredictedMoves();

            updateDebugText();

            // TODO : REMOVE (TEST)
            /*int eval = bot->evaluatePosition(board);
            if (!board.whiteTurn) {
                eval = -eval;
            }
            setEvalValue(eval);*/
        }

        void undoMove() {
            if (moveHistory.size() < 1) {
                return;
            }

            Move move = moveHistory[moveHistory.size() - 1];
            UnmakeMoveInfo info = unmakeInfos[unmakeInfos.size() - 1];

            highlitedSquares.clear();

            highlitedSquares.push_back({(char) squareX(move.startSquare), (char) squareY(move.startSquare)});
            highlitedSquares.push_back({(char) squareX(move.endSquare), (char) squareY(move.endSquare)});

            board.undoMove(move, info);

            moveHistory.pop_back();
            unmakeInfos.pop_back();

            bot->onMoveUndone(board);

            predictedVariation.clear();
            restorePredictedMoves();

            updateDebugText();

            // Adjust position of moved piece to animate it
            int screenX, screenY;
            boardPosToScreenPos(squareX(move.endSquare), squareY(move.endSquare), screenX, screenY);
            piecesDisplayPosX[move.startSquare] = (float) screenX;
            piecesDisplayPosY[move.startSquare] = (float) screenY;
        }

        // Sets a classification for the current move, that will be displayed
        void setClassification(MoveClassification moveClassification) {
            currentMoveClassification = moveClassification;
        }

        void onElementsLoaded() {
            shared->elements->evaluationText.setPos({
                evaluationBarRect.x + evaluationBarRect.w/2, 
                evaluationBarRect.y + evaluationBarRect.h - 20
            });

            updateDebugText();
        }

        inline bool isHoldingPiece() {
            return holdPieceMoves.size() > 0;
        }

        void setEvalValue(int botEval) {
            botEvaluation = std::clamp(((float) botEval) / 100.0f, -500.0f, 500.0f);

            float roundedEval = std::round(botEvaluation * 10.0f) / 10.0f;

            if (roundedEval == 0.0f) {
                shared->elements->evaluationText.setText("0.0");
            } else if (std::abs(botEval) > CHECKMATE_THRESHOLD) {
                std::string evalText = "M ";
                evalText += std::to_string(CHECKMATE_BASE_SCORE - std::abs(botEval));
                shared->elements->evaluationText.setText(evalText.c_str());
            } else {
                std::string evalText = std::to_string(roundedEval);
                // Removes traling zeros
                evalText.erase(evalText.find_last_not_of('0') + 1, std::string::npos);
                evalText.erase(evalText.find_last_not_of('.') + 1, std::string::npos);
                shared->elements->evaluationText.setText(evalText.c_str());
            }
        }
    
    protected:
        SDL_Renderer* renderer;

        SDL_Rect boardRect;
        SDL_Rect boardOutline1;
        SDL_Rect boardOutline2;

        SDL_Rect evaluationBarRect;
        SDL_Rect evaluationBarOutline;

        SDL_Rect cellRect;
        SDL_Rect promotionRect;
        SDL_Rect captureIconRect;

        SDL_Rect moveClassificationRect;

        SDL_Texture* whiteTextures[PIECE_TYPE_COUNT];
        SDL_Texture* blackTextures[PIECE_TYPE_COUNT];

        SDL_Texture* whitePromotionTextures[PIECE_TYPE_COUNT];
        SDL_Texture* blackPromotionTextures[PIECE_TYPE_COUNT];

        SDL_Texture* whiteCaptureTextures[PIECE_TYPE_COUNT];
        SDL_Texture* blackCaptureTextures[PIECE_TYPE_COUNT];

        SDL_Texture* captureCircleTexture;
        SDL_Texture* moveCircleTexture;

        SDL_Texture* moveClassificationTextures[MOVE_CLASSIFICATION_COUNT];

        SDL_Rect circleRect;

        BoardPos holdPiecePos;

        bool isPromotionAsked = false;
        BoardPos askedPromotionStartPos = {0, 0};
        BoardPos askedPromotionEndPos = {0, 0};

        std::vector<Move> holdPieceMoves = {};

        std::vector<BoardPos> highlitedSquares = {};

        float botEvaluation = 0.0f;
        float displayBotEvaluation = 0.0f;

        std::vector<UnmakeMoveInfo> unmakeInfos;
        std::vector<Move> moveHistory;
        MoveClassification currentMoveClassification = NO_CLASSIFICATION;

        std::array<SDL_Rect, PROMOTION_PIECES_COUNT> getPromotionRects( const BoardPos &pos ) {
            int x = pos.x * cellRect.w + boardRect.x;
            int y = pos.y * cellRect.w + boardRect.y;

            return {{
                {x, y,
                 promotionRect.w, promotionRect.h},
                {x + promotionRect.w, y,
                 promotionRect.w, promotionRect.h},
                {x, y + promotionRect.h,
                 promotionRect.w, promotionRect.h},
                {x + promotionRect.w, y + promotionRect.h,
                 promotionRect.w, promotionRect.h}
            }};
        }

        void drawPiece(const Piece &piece, const SDL_Rect &rect) {
            if (piece.type != EMPTY) {
                if (piece.isWhite) {
                    SDL_RenderCopy(renderer, whiteTextures[piece.type], NULL, &rect);
                } else {
                    SDL_RenderCopy(renderer, blackTextures[piece.type], NULL, &rect);
                }
            }
        }

        void drawPromotionPiece(const Piece &piece, const int xMouse, const int yMouse) {
            if (piece.type != EMPTY) {
                SDL_Point point {xMouse, yMouse};
                if ( SDL_PointInRect(&point, &promotionRect) ) {
                    shared->currentCursor = shared->CURSOR_HAND;
                    SDL_SetRenderDrawColor( renderer, 45, 196, 45, 255 );
                } else {
                    SDL_SetRenderDrawColor( renderer, 120, 227, 93, 255 );
                }
                SDL_RenderFillRect( renderer, &promotionRect );

                if (piece.isWhite) {
                    SDL_RenderCopy(renderer, whitePromotionTextures[piece.type], NULL, &promotionRect);
                } else {
                    SDL_RenderCopy(renderer, blackPromotionTextures[piece.type], NULL, &promotionRect);
                }
            }
        }

        BoardPos posToBoard(const int mouseX, const int mouseY) {
            return BoardPos{(char) ((mouseX - boardRect.x)/cellRect.w), (char) ((mouseY - boardRect.y)/cellRect.w)};
        }

        SDL_Texture* loadTexture(const char* file, const int width, const int height) {
            // Best is sadly linear here
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

            SDL_Texture* auxTexture = IMG_LoadTexture(renderer, file);
            SDL_Texture* texture = SDL_CreateTexture(renderer, 
                                                     SDL_PIXELFORMAT_RGBA32, 
                                                     SDL_TEXTUREACCESS_TARGET, 
                                                     width,
                                                     height);

            SDL_SetTextureScaleMode(auxTexture, SDL_ScaleModeBest);
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(renderer, texture);
            SDL_RenderCopy(renderer, auxTexture, NULL, NULL);

            SDL_DestroyTexture(auxTexture);

            SDL_SetRenderTarget(renderer, NULL);

            return texture;
        }

        void loadTextures() {
            for (int i = 0 ; i < PIECE_TYPE_COUNT ; i++) {
                whiteTextures[i] = loadTexture(whiteFiles[i], cellRect.w, cellRect.h);
                blackTextures[i] = loadTexture(blackFiles[i], cellRect.w, cellRect.h);

                whiteCaptureTextures[i] = loadTexture(whiteFiles[i], captureIconSize, captureIconSize);
                blackCaptureTextures[i] = loadTexture(blackFiles[i], captureIconSize, captureIconSize);
            }
            for (int i = 0 ; i < PROMOTION_PIECES_COUNT ; i++) {
                whitePromotionTextures[promotionTypes[i]] = loadTexture(whiteFiles[promotionTypes[i]], promotionRect.w, promotionRect.h);
                blackPromotionTextures[promotionTypes[i]] = loadTexture(blackFiles[promotionTypes[i]], promotionRect.w, promotionRect.h);
            }
            captureCircleTexture = loadTexture(captureCirceFile, circleRect.w, circleRect.w);
            moveCircleTexture = loadTexture(moveCircleFile, circleRect.w, circleRect.w);

            for (int i = 0 ; i < MOVE_CLASSIFICATION_COUNT ; i++) {
                moveClassificationTextures[i] = loadTexture(classificationFiles[i], moveClassificationRect.w, moveClassificationRect.h);
            }
        }

        void updateDebugText() {
            std::string zobristText = std::format("Zobrist Hash : ({:x})", board.zobristHash);
            shared->elements->zobristHashText.setText(zobristText.c_str());

            std::string depthText = std::format("Depth : {}", bot->currentDepth);
            shared->elements->depthText.setText(depthText.c_str());

            #if MESURE_LEVEL >= LOW_MESURE
            std::string text = std::format("Noeuds : {}k  ({} kNPS)", bot->nodeCount/1000, bot->kNPS);
            shared->elements->counterText.setText(text.c_str());
            #endif
        }

        // Supposed to but run into a thread
        // Should not be run when another search is currently being made !
        static void startBotSearch(Game * game, Bot * bot, const Board * board) {
            if (bot == NULL) {return;}
            Board boardCopy = board->copy();

            MoveResult bestResult = bot->getBestMove(boardCopy, true, true, false);

            game->botThreadMutex.lock();
            game->obtainedResult = bestResult;
            game->botThreadMutex.unlock();
        }
};


#endif
