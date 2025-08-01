#ifndef GAME
#define GAME

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include "shared.cpp"
#include "engine/board.cpp"
#include "UIConstants.cpp"
#include "engine/gameConstants.cpp"
#include "engine/bot.cpp"
#include <array>
#include <vector>
#include <memory>
#include <chrono>


class Game {
    public:
        Board board;
        Shared * shared;

        Game() = delete;
        Game(SDL_Renderer* renderer, Shared * shared) : renderer(renderer)  {
            this->shared = shared;

            board = Board();
            
            boardRect = {(int) (SCREEN_WIDTH - SCREEN_HEIGHT * 0.8)/2, (int) (SCREEN_HEIGHT * 0.1), (int) (SCREEN_HEIGHT * 0.8), (int) (SCREEN_HEIGHT * 0.8)};
            boardOutline1 = {boardRect.x - 8, boardRect.y - 8, boardRect.w + 16, boardRect.h + 16};
            boardOutline2 = {boardRect.x - 6, boardRect.y - 6, boardRect.w + 12, boardRect.h + 12};

            evaluationBarRect = {boardRect.x - 50, boardRect.y, 20, boardRect.h};
            evaluationBarOutline = {evaluationBarRect.x - 2, evaluationBarRect.y - 2, evaluationBarRect.w + 4, evaluationBarRect.h + 4};

            cellRect = {0, 0, boardRect.w / ROW_COUNT, boardRect.w / ROW_COUNT};
            promotionRect = {boardRect.x, boardRect.y, cellRect.w / 2, cellRect.w / 2};
            
            circleRect = {boardRect.x, boardRect.y, (int) (cellRect.w * 0.6), (int) (cellRect.w * 0.6)};

            captureIconRect = {0, 0, captureIconSize, captureIconSize};

            loadTextures();
        };

        void update() {
            int xMouse, yMouse;
            SDL_GetMouseState( &xMouse, &yMouse );

            drawBoard(xMouse, yMouse);
            
            if ( holdPieceMoves.size() > 0 ) {
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
            for (char y = 0 ; y < ROW_COUNT ; y++) {
                for (char x = 0 ; x < ROW_COUNT ; x++) {
                    moveRectToBoardPos(cellRect, x, y);

                    if ( !isPromotionAsked || (askedPromotionStartPos.x != x || askedPromotionStartPos.y != y) ) {
                        if ( holdPieceMoves.size() == 0 || x != holdPiecePos.x || y != holdPiecePos.y ) {
                            drawPiece( board.getAt(x, y) );
                        }
                    }
                }
            }

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

        void moveRectToBoardPos(SDL_Rect &rect, char x, char y) {
            rect.x = x * cellRect.w + boardRect.x;
            rect.y = y * cellRect.w + boardRect.y;
        }

        void drawMoves(int xMouse, int yMouse) {
            for (const Move move : holdPieceMoves) {
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
            drawPiece( board.getAt(holdPiecePos) );
        }

        void drawEvalutionBar() {
            displayBotEvaluation += (botEvaluation-displayBotEvaluation)/50.0f;

            SDL_SetRenderDrawColor( renderer, 128, 128, 128, 255 );
            SDL_RenderFillRect( renderer, &evaluationBarOutline );

            float whiteProgress;
            if (displayBotEvaluation < 0) {
                whiteProgress = -1.0f/(displayBotEvaluation - 2.0f);
            } else {
                whiteProgress = 1.0f - 1.0f/(displayBotEvaluation + 2.0f);
            }
            
            SDL_Rect blackRegionRect = {evaluationBarRect.x, evaluationBarRect.y, evaluationBarRect.w, (int) std::round(evaluationBarRect.h * (1.0f - whiteProgress))};
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
            int alpha = 200;

            for (const Move &move : principalVariation) {
                BoardPos startPos = {(char) squareX(move.startSquare), (char) squareY(move.startSquare)};
                BoardPos endPos = {(char) squareX(move.endSquare), (char) squareY(move.endSquare)};

                drawArrow(startPos, endPos, width, alpha);
                
                width -= 1;
                alpha -= 20;

                if (width <= 3) {
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

            /*238, 131, 183*/
            /*rgba(238, 234, 131, 1)*/

            thickLineRGBA(renderer, startX, startY, startX + dx*0.97f, startY + dy*0.97f, width, 238, 234, 131, alpha);

            float widness = d*0.5f;
            float tipLength = width*4.0f;

            float xA = startX - dy*widness/d;
            float yA = startY + dx*widness/d;
            float d2 = std::sqrt((xA - endX)*(xA - endX) + (yA - endY)*(yA - endY));
            
            float xB = startX + dy*widness/d;
            float yB = startY - dx*widness/d;

            // thickLineRGBA(renderer, endX, endY, endX + (xA - endX)*tipLength/d2, endY + (yA - endY)*tipLength/d2, width-1, 238, 131, 183, alpha);
            // thickLineRGBA(renderer, endX, endY, endX + (xB - endX)*tipLength/d2, endY + (yB - endY)*tipLength/d2, width-1, 238, 131, 183, alpha);
        
            filledTrigonRGBA(
                renderer, 
                endX, endY, 
                endX + (xA - endX)*tipLength/d2, endY + (yA - endY)*tipLength/d2,
                endX + (xB - endX)*tipLength/d2, endY + (yB - endY)*tipLength/d2,
                238, 234, 131, 255
            );
            aatrigonRGBA(
                renderer, 
                endX, endY, 
                endX + (xA - endX)*tipLength/d2, endY + (yA - endY)*tipLength/d2,
                endX + (xB - endX)*tipLength/d2, endY + (yB - endY)*tipLength/d2,
                238, 234, 131, 255
            );
        }

        void mouseDown(const int mouseX, const int mouseY) {
            if (board.state != NEUTRAL) {
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
            if (board.state != NEUTRAL) {
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

        void botPlays() {
            shared->currentCursor = shared->CURSOR_WAIT_ARROW;
            shared->update();

            printf("----- Bot plays -----\n");

            MoveResult bestResult = getBestMove(board);

            if (bestResult.move == NO_MOVE) {
                printf("----- The game is over -----\n");
                return;
            }
            
            playMove(bestResult.move);

            botEvaluation = bestResult.score;

            printf("| Board Evaluation: %f\n", botEvaluation);

            std::string text = "Noeuds : ";
            text += std::to_string(nodeCount);
            shared->elements->counterText.setText(text.c_str());

            // FOR DEBUG
            printf("| Principal variation:\n");
            for (Move move : principalVariation) {
                printMove(move);
            }
        }

        void playMove(const Move &move) {
            highlitedSquares.clear();

            highlitedSquares.push_back({(char) squareX(move.startSquare), (char) squareY(move.startSquare)});
            highlitedSquares.push_back({(char) squareX(move.endSquare), (char) squareY(move.endSquare)});

            moveHistory.push_back(move);
            unmakeInfos.push_back(board.playMove(move));
            onMovePlayed(board);
        }

        void undoMove() {
            if (moveHistory.size() == 0) {
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

            onMoveUndone(board);
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

        SDL_Texture* whiteTextures[PIECE_TYPE_COUNT];
        SDL_Texture* blackTextures[PIECE_TYPE_COUNT];

        SDL_Texture* whitePromotionTextures[PIECE_TYPE_COUNT];
        SDL_Texture* blackPromotionTextures[PIECE_TYPE_COUNT];

        SDL_Texture* whiteCaptureTextures[PIECE_TYPE_COUNT];
        SDL_Texture* blackCaptureTextures[PIECE_TYPE_COUNT];

        SDL_Texture* captureCircleTexture;
        SDL_Texture* moveCircleTexture;
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

        void drawPiece(const Piece piece) {
            if (piece.type != EMPTY) {
                if (piece.isWhite) {
                    SDL_RenderCopy(renderer, whiteTextures[piece.type], NULL, &cellRect);
                } else {
                    SDL_RenderCopy(renderer, blackTextures[piece.type], NULL, &cellRect);
                }
            }
        }

        void drawPromotionPiece(const Piece piece, const int xMouse, const int yMouse) {
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
        }
};

#endif
