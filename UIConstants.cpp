#ifndef UI_CONSTANTS
#define UI_CONSTANTS

int const SCREEN_WIDTH{1080};
int const SCREEN_HEIGHT{720 + 40};


const char* whiteFiles[] {
    "assets/pieces/100/whitePawn.png",
    "assets/pieces/100/whiteBishop.png",
    "assets/pieces/100/whiteKnight.png",
    "assets/pieces/100/whiteRook.png",
    "assets/pieces/100/whiteQueen.png",
    "assets/pieces/100/whiteKing.png"
};
const char* blackFiles[] {
    "assets/pieces/100/blackPawn.png",
    "assets/pieces/100/blackBishop.png",
    "assets/pieces/100/blackKnight.png",
    "assets/pieces/100/blackRook.png",
    "assets/pieces/100/blackQueen.png",
    "assets/pieces/100/blackKing.png"
};

const char* captureCirceFile = "assets/UI/captureCircle.png";
const char* moveCircleFile = "assets/UI/moveCircle.png";

const int captureIconSize = 40;
const int captureIconPadding = 5;

const int moveClassificationSize = 30;

const char* classificationFiles[] {
    "assets/moveClassifications/50/brilliant.png",
    "assets/moveClassifications/50/great.png",
    "assets/moveClassifications/50/best.png",
    "assets/moveClassifications/50/excellent.png",
    "assets/moveClassifications/50/ok.png",
    "assets/moveClassifications/50/inaccuracy.png",
    "assets/moveClassifications/50/mistake.png",
    "assets/moveClassifications/50/missed.png",
    "assets/moveClassifications/50/blunder.png"
};

#endif
