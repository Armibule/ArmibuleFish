#ifndef NNUE_CPP
#define NNUE_CPP

#include "board.cpp"
#include <math.h>
#include <fstream>
#include <stdexcept>



// The quantized network used for evaluating games in the bot

// Every weight is multiplied by this constant
const int16_t QUANTIZE = 63;
const int INPUT_LAYER_SIZE = 2 * 6 * 64;
const int ACCUMULATOR_SIZE = 256; // 128; // 256;

const int OUTPUT_BUCKETS = 30; // 16;
const int PIECE_PER_BUCKET = (30 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;    // Does not include kings, rounded up

const int centipawnsConversion = 100;

// Maximum number of pieces additions made to the accumulator (max is on castling)
const int MAX_ACCUMULATOR_ADDSUB = 2;


// const /*short*/int  invertPerspectiveXor = ((6*64) ^ 0b111000);        // blackIndex = whiteIndex ^ inputToBlackPerpectiveXor


const short toBlackInput(short whiteInput) {
    whiteInput ^= 0b111000;
    if (whiteInput > 6*64) {
        return (whiteInput - 6*64);
    }

    return (whiteInput + 6*64);
}

/*inline short pieceInputIndex(const Piece &piece, Square square, bool whitePerspective) {
    int a = (piece.isWhite*(6*64) + piece.type*64 + square)^invertPerspectiveXor;
    int b = (!piece.isWhite)*(6*64) + piece.type*64 + (square^0b111000);
    if (a != b) {
        std::cout << a << std::endl;
        std::cout << b << std::endl;
        throw;
    }

    if (whitePerspective) {
        return piece.isWhite*(6*64) + piece.type*64 + square;
    }
    return (!piece.isWhite)*(6*64) + piece.type*64 + (square^0b111000);
}*/

// Assumes piece is not empty
inline short pieceInputIndex(const Piece &piece, Square square, bool whitePerspective) {
    /*int a = (piece.isWhite*(6*64) + piece.type*64 + square)^invertPerspectiveXor;
    int b = (!piece.isWhite)*(6*64) + piece.type*64 + (square^0b111000);
    if (a != b) {
        std::cout << a << std::endl;
        std::cout << b << std::endl;
        throw;
    }*/

    if (whitePerspective) {
        return piece.isWhite*(6*64) + piece.type*64 + square;
    }
    return (!piece.isWhite)*(6*64) + piece.type*64 + (square^0b111000);
}


class NNUE;
// To apply and revert the changes made to the accumulator after the move is undone
class AccumulatorChanges {
    public:

    char addCount = 0;
    char removeCount = 0;

    // Should not be used more than two times !
    void pushAdd(const Piece &piece, Square square) {
        addedPieces[addCount] = piece;
        addedPiecesSquares[addCount] = square;

        //addedInputIndexesWhite[addCount] = pieceInputIndex(piece, square, true);
        addCount += 1;
    }
    // Should not be used more than two times !
    void pushSub(const Piece &piece, Square square) {
        removedPieces[removeCount] = piece;
        removedPiecesSquares[removeCount] = square;

        //removedInputIndexesWhite[removeCount] = pieceInputIndex(piece, square, true);
        removeCount += 1;

        //sizeof(AccumulatorChanges);
    }
    
    private:

    friend NNUE;
    Piece addedPieces[MAX_ACCUMULATOR_ADDSUB];
    Piece removedPieces[MAX_ACCUMULATOR_ADDSUB];
    Square addedPiecesSquares[MAX_ACCUMULATOR_ADDSUB];
    Square removedPiecesSquares[MAX_ACCUMULATOR_ADDSUB];

    // Indexes from white perspective
    /*short addedInputIndexesWhite[MAX_ACCUMULATOR_ADDSUB];
    short removedInputIndexesWhite[MAX_ACCUMULATOR_ADDSUB];*/
};


struct Accumulator {
    // The activation functions isn't applied here -> efficient updates
    int16_t values[ACCUMULATOR_SIZE] = {};
};


/*
Side to play  ->  Accumulator ->  Output

On black's turn the board is mirrored and colors are inverted
*/
class NNUE {
    public :

    // Should be the accumulator corresponding to the side which plays
    int feedForward(const Accumulator &accumulatorPlaying, const Accumulator &accumulatorOpponent, int outputBucketIndex) const {
        const int16_t * outputWeightsPlaying = &outputWeightsBuckets[outputBucketIndex][0];
        const int16_t * outputWeightsOpponent = &outputWeightsBuckets[outputBucketIndex][ACCUMULATOR_SIZE];

        const int16_t * playingValues = &accumulatorPlaying.values[0];
        const int16_t * opponentValues = &accumulatorOpponent.values[0];

        int outputActivation = outputBiasBuckets[outputBucketIndex];

        /*for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
            outputActivation += ClampedReLU_QuantizedAccumulator(accumulatorPlaying.values[i]) * outputWeights[i];
        }
        for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
            outputActivation += ClampedReLU_QuantizedAccumulator(accumulatorOpponent.values[i]) * outputWeights[i+ACCUMULATOR_SIZE];
        }*/ 
        
        /*for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
            outputActivation += ClampedReLU_QuantizedAccumulator(accumulatorPlaying.values[i]) * outputWeights[i];
        //} for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
            outputActivation += ClampedReLU_QuantizedAccumulator(accumulatorOpponent.values[i]) * outputWeights[i+ACCUMULATOR_SIZE];
        }*/

        for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
            outputActivation += ClampedReLU_QuantizedAccumulator(playingValues[i]) * outputWeightsPlaying[i];
        // } for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
            outputActivation += ClampedReLU_QuantizedAccumulator(opponentValues[i]) * outputWeightsOpponent[i];
        }
        
        return (outputActivation * centipawnsConversion) / (QUANTIZE*QUANTIZE);
    }

    void applyAccumulatorChanges(Accumulator &destAccumulator, bool whiteAccumulatorPerspective, const AccumulatorChanges &accumulatorChanges) const {
        for (int n = 0 ; n < accumulatorChanges.addCount ; n++) {
            int inputIndex = pieceInputIndex(accumulatorChanges.addedPieces[n], accumulatorChanges.addedPiecesSquares[n], whiteAccumulatorPerspective);
            /*int inputIndex = accumulatorChanges.addedInputIndexesWhite[n];
            if (!whiteAccumulatorPerspective) {
                inputIndex = toBlackInput(inputIndex);
            }*/
            
            const int16_t * weights = accumulatorWeights[inputIndex];

            for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
                destAccumulator.values[i] += weights[i];
            }
        }
        for (int n = 0 ; n < accumulatorChanges.removeCount ; n++) {
            int inputIndex = pieceInputIndex(accumulatorChanges.removedPieces[n], accumulatorChanges.removedPiecesSquares[n], whiteAccumulatorPerspective);
            /*int inputIndex = accumulatorChanges.removedInputIndexesWhite[n];
            if (!whiteAccumulatorPerspective) {
                inputIndex = toBlackInput(inputIndex);
            }*/
            
            const int16_t * weights = accumulatorWeights[inputIndex];

            for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
                destAccumulator.values[i] -= weights[i];
            }
        }
    }

    // Use copy make instead
    /*void undoAccumulatorChanges(Accumulator &accumulator, bool whiteAccumulatorPerspective, const AccumulatorChanges &accumulatorChanges) const {
        for (int n = 0 ; n < accumulatorChanges.addCount ; n++) {
            int inputIndex = pieceInputIndex(accumulatorChanges.addedPieces[n], accumulatorChanges.addedPiecesSquares[n], whiteAccumulatorPerspective);
            const int16_t * weights = accumulatorWeights[inputIndex];

            for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
                accumulator.values[i] -= weights[i];
            }
        }
        for (int n = 0 ; n < accumulatorChanges.removeCount ; n++) {
            int inputIndex = pieceInputIndex(accumulatorChanges.removedPieces[n], accumulatorChanges.removedPiecesSquares[n], whiteAccumulatorPerspective);
            const int16_t * weights = accumulatorWeights[inputIndex];

            for (int i = 0 ; i < ACCUMULATOR_SIZE ; i++) {
                accumulator.values[i] += weights[i];
            }
        }
    }*/

    // Loads an accumulator perspective from a Board, slow
    inline void boardToAccumulator(Accumulator &accumulator, const Piece pieces[64], bool whitePerpective) {
        for (int accIndex = 0 ; accIndex < ACCUMULATOR_SIZE ; accIndex++) {
            accumulator.values[accIndex] = accumulatorBiases[accIndex];
        }
        
        for (Square square = 0 ; square < 64 ; square++) {
            Piece piece = pieces[square];

            if (piece.type != EMPTY) {
                int inputIndex = pieceInputIndex(piece, square, whitePerpective);

                for (int accIndex = 0 ; accIndex < ACCUMULATOR_SIZE ; accIndex++) {
                    accumulator.values[accIndex] += accumulatorWeights[inputIndex][accIndex];
                }
            }
        }
    }

    inline static int outputBucketIndex(uint64_t allOccupancy) {
        // Normal
        // return (std::popcount(allOccupancy) - 2) / PIECE_PER_BUCKET;

        // Special
        return std::popcount(allOccupancy) - 3;
    }

    // Loads .nnue file, Don't forget to free the pointer
    static NNUE * fromFile(const char * fileName) {
        NNUE * network = new NNUE();

        std::ifstream file (fileName, std::ios_base::binary);
        if (!file.good()) {
            throw std::invalid_argument("The NNUE file is not found !");
        }

        file.read((char *) network->accumulatorWeights, sizeof(int16_t) * ACCUMULATOR_SIZE*INPUT_LAYER_SIZE);
        file.read((char *) network->accumulatorBiases, sizeof(int16_t) * ACCUMULATOR_SIZE);
        file.read((char *) network->outputWeightsBuckets, sizeof(int16_t) * OUTPUT_BUCKETS*ACCUMULATOR_SIZE*2);
        file.read((char *) &network->outputBiasBuckets, sizeof(int16_t) * OUTPUT_BUCKETS);
        file.close();

        return network;
    }

    // Weights are transposed for better performance
    int16_t accumulatorWeights[INPUT_LAYER_SIZE][ACCUMULATOR_SIZE];
    int16_t accumulatorBiases[ACCUMULATOR_SIZE];

    int16_t outputWeightsBuckets[OUTPUT_BUCKETS][ACCUMULATOR_SIZE*2];
    int16_t outputBiasBuckets[OUTPUT_BUCKETS];

    private:

    inline int16_t ClampedReLU_QuantizedAccumulator(int16_t x) const {
        if (x <= 0) {
            return 0;
        }
        if (x >= QUANTIZE) {
            return QUANTIZE;
        }

        return x;
    }

    NNUE() {}
};

#endif
