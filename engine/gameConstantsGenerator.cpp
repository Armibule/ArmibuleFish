#ifndef GAME_CONSTANTS_GENERATOR
#define GAME_CONSTANTS_GENERATOR

#include <cstdint>
#include <vector>
#include <bit>
#include <iostream>
#include <unordered_map>
#include <random>
#include <chrono>
#include "generatedConstants.cpp"
#include <x86intrin.h>


inline uint64_t bit(int x, int y) {
    // return 1ULL << ((7ULL - x) + 8ULL*(7ULL - y));
    // Faster
    return 1ULL << ( 63ULL - x - 8ULL*y );
}
inline uint64_t bit(int square) {
    return 1ULL << ( 63ULL - square );
} 


// Indexes start from the most significant bit
// TODO : could maybe be improved by using an array instead of a vector
/*inline void bitscan(uint64_t x, std::vector<char> &indexes) {
    char i = 0;
    
    // indexes.reserve(std::popcount(x));
    while (x) {
        if (x & 1ULL) {
            indexes.push_back(63 - i);
        }

        x >>= 1ULL;
        i += 1;
    }
}*/
inline void bitscan(uint64_t x, std::vector<char> &indexes) {
    // indexes.reserve(std::popcount(x));
    uint64_t LS1B;
    while (x) {
        LS1B = x & (-x);
        indexes.push_back(_lzcnt_u64(LS1B));
        x ^= LS1B;
    }
}


void printBB(uint64_t bitBoard) {
    for (int i = 0 ; i < 8 ; i++) {
        for (int j = 0 ; j < 8 ; j++) {
            printf("%d", 1 & (bitBoard >> (63 - 8*i - j)));
        }
        printf("\n");
    }
    printf("\n");
}

uint64_t kingAttacks[64] = {};
uint64_t knightAttacks[64] = {};

// uint64_t whitePawnAttacks[64] = {};
// uint64_t blackPawnAttacks[64] = {};
// [blackPawnAttacks, whitePawnAttacks]
uint64_t colorsPawnAttacks[2][64] = {};

// uint64_t columnMask[64] = {};
// uint64_t rowMask[64] = {};
uint64_t rookMasks[64] = {};
uint64_t bishopMasks[64] = {};


inline void tryAttack(uint64_t &bitBoard, char x, char y) {
    if (0 <= x && x < 8 && 0 <= y && y < 8) {
        bitBoard |= bit(x, y);
    }
}


uint64_t genKingAttacks(char x, char y) {
    uint64_t bitBoard = 0;

    tryAttack(bitBoard, x-1, y-1);
    tryAttack(bitBoard, x,   y-1);
    tryAttack(bitBoard, x+1, y-1);
    tryAttack(bitBoard, x-1, y);
    tryAttack(bitBoard, x,   y);
    tryAttack(bitBoard, x+1, y);
    tryAttack(bitBoard, x-1, y+1);
    tryAttack(bitBoard, x,   y+1);
    tryAttack(bitBoard, x+1, y+1);

    return bitBoard;
}


uint64_t genKnightAttacks(char x, char y) {
    uint64_t bitBoard = 0;

    tryAttack(bitBoard, x-1, y-2);
    tryAttack(bitBoard, x+1, y-2);
    tryAttack(bitBoard, x-1, y+2);
    tryAttack(bitBoard, x+1, y+2);

    tryAttack(bitBoard, x-2, y-1);
    tryAttack(bitBoard, x-2, y+1);
    tryAttack(bitBoard, x+2, y-1);
    tryAttack(bitBoard, x+2, y+1);

    return bitBoard;
}

uint64_t genWhitePawnAttacks(char x, char y) {
    uint64_t bitBoard = 0;

    tryAttack(bitBoard, x-1, y-1);
    tryAttack(bitBoard, x+1, y-1);

    return bitBoard;
}

uint64_t genBlackPawnAttacks(char x, char y) {
    uint64_t bitBoard = 0;

    tryAttack(bitBoard, x-1, y+1);
    tryAttack(bitBoard, x+1, y+1);

    return bitBoard;
}

uint64_t genColumnMask(char x, char y) {
    uint64_t bitBoard = 0;

    char py = y+1;
    while (py < 7) {
        tryAttack(bitBoard, x, py);
        py += 1;
    }

    py = y-1;
    while (py > 0) {
        tryAttack(bitBoard, x, py);
        py -= 1;
    }

    return bitBoard;
}

uint64_t genRowMask(char x, char y) {
    uint64_t bitBoard = 0;

    char px = x+1;
    while (px < 7) {
        tryAttack(bitBoard, px, y);
        px += 1;
    }

    px = x-1;
    while (px > 0) {
        tryAttack(bitBoard, px, y);
        px -= 1;
    }

    return bitBoard;
}

uint64_t genRookMask(char x, char y) {
    return genRowMask(x, y) | genColumnMask(x, y);
}

uint64_t genBishopMask(char x, char y) {
    uint64_t bitBoard = 0;

    char py = y+1;
    char px = x+1;
    while (py < 7 && px < 7) {
        bitBoard |= bit(px, py);
        py += 1;
        px += 1;
    }
    py = y-1;
    px = x+1;
    while (py >= 1 && px < 7) {
        bitBoard |= bit(px, py);
        py -= 1;
        px += 1;
    }
    py = y-1;
    px = x-1;
    while (py >= 1 && px >= 1) {
        bitBoard |= bit(px, py);
        py -= 1;
        px -= 1;
    }
    py = y+1;
    px = x-1;
    while (py < 7 && px >= 1) {
        bitBoard |= bit(px, py);
        py += 1;
        px -= 1;
    }

    return bitBoard;
}


void generateAttacks(uint64_t array[64], uint64_t (* gen)(char x, char y)) {
    int i = 0;
    for (char y = 0 ; y < 8 ; y++) {
        for (char x = 0 ; x < 8 ; x++) {
            array[i] = (gen)(x, y);

            i += 1;
        }
    }
}

std::mt19937_64 random (std::chrono::steady_clock::now().time_since_epoch().count());

// Initialisation functions
void genBitboardConstants() {
    generateAttacks(kingAttacks, genKingAttacks);
    generateAttacks(knightAttacks, genKnightAttacks);

    generateAttacks(colorsPawnAttacks[1] /*whitePawnAttacks*/, genWhitePawnAttacks);
    generateAttacks(colorsPawnAttacks[0] /*blackPawnAttacks*/, genBlackPawnAttacks);

    // generateAttacks(columnMask, genColumnMask);
    // generateAttacks(rowMask, genRowMask);
    generateAttacks(rookMasks, genRookMask);
    generateAttacks(bishopMasks, genBishopMask);
}


// Out-of-runtime functions
void occupencyPossibilities(uint64_t mask, uint64_t occupencies[]) {
    std::vector<char> indexes = {};
    bitscan(mask, indexes);

    int indexCount = indexes.size();
    int possibleCount = 1<<indexCount;
    
    for (int i = 0 ; i < possibleCount ; i++) {
        uint64_t occupency = 0;

        for (int j = 0 ; j < indexCount ; j++) {
            if (i & (1 << j)) {
                occupency |= 1ULL << (63 - indexes[j]);
            }
        }

        occupencies[i] = occupency;
    }
}

int64_t mapIndexToMask(int index, int64_t mask, int bits) {
    std::vector<char> positions = {};
    bitscan(mask, positions);

    int bitCount = positions.size();
    int indexRange = 1ULL << bitCount;

    int64_t result = 0;
    
    for (int i = 0 ; i < bits ; i++) {
        if (index & (1ULL << i)) {
            result |= positions[i];
        }
    }

    return result;
}

inline int applyMagic(uint64_t occupency, uint64_t magic, int bitSize) {
    return (occupency * magic) >> (64 - bitSize);
}

/*bool testMagic(std::unordered_map<uint64_t, uint64_t> &correctMap, uint64_t magic, int bitSize) {
    uint64_t mask = (1ULL<<bitSize) - 1ULL;

    // {bits: correctResult, ...}
    std::unordered_map<uint64_t, uint64_t> map = {};

    for (auto item : correctMap) {
        uint64_t possible = item.first;
        uint64_t correct = item.second;

        uint64_t bits = applyMagic(possible, magic, bitSize);
        //uint64_t bits = possible;

        if (map.find(bits) == map.end()) {
            map[bits] = correct;
        } else if (map[bits] != correct) {
            return false;
        }
    }

    return true;
}*/

/*uint64_t findMagic(std::unordered_map<uint64_t, uint64_t> &correctMap, int bitSize) {
    std::mt19937_64 random (std::chrono::steady_clock::now().time_since_epoch().count());
    uint64_t magic;
    int testCount = 0;
    do {
        // Low number of 1 is good
        magic = random() & random() & random();
        if (std::popcount(magic) < 6) {
            continue;
        }

        testCount += 1;

        //if (testCount % 1048576 == 0) {
        if (testCount % 1024 == 0) {
            printf("Tested: %d\n", testCount);
        }
        //printf("Tested: %d\n", testCount);
    } while (!testMagic(correctMap, magic, bitSize));

    return magic;
}*/

bool testMagic(uint64_t * occupencies, uint64_t * attacks, uint64_t magic, int hashBits, uint64_t * table, int maskSize) {
    int tableSize = 1 << hashBits;
    int occupenciesCount = 1 << maskSize;

    for (int i = 0 ; i < tableSize ; i++) {
        table[i] = 0;
    }

    uint64_t bits;
    for (int i = 0 ; i < occupenciesCount ; i++) {
        bits = applyMagic(occupencies[i], magic, hashBits);

        if (table[bits] == 0ULL) {
            table[bits] = attacks[i];
        } else if (table[bits] != attacks[i]) {
            // printf("i=%d\n", i);
            return false;
        }
    }

    return true;
}

uint64_t findMagic(uint64_t * occupencies, uint64_t * attacks, int hashBits, uint64_t mask, int maskSize) {
    uint64_t magic;
    int testCount = 0;

    int tableSize = 1 << hashBits;
    uint64_t table[tableSize] = {};

    do {
        // Low number of 1 is better
        magic = random() & random() & random();
        // printf("%llx\n", magic);
        if (std::popcount((magic * mask) & 0xFF00000000000000ULL) < 5) {
            continue;
        }

        testCount += 1;
        if (testCount % 1048576 == 0) {
            printf("Tested: %d\n", testCount);
        }
    } while (!testMagic(occupencies, attacks, magic, hashBits, table, maskSize));

    return magic;
}

void genRookMagic() {
    char x = 0;
    char y = 0;

    std::vector<int> ptrIndexes = {};
    std::vector<uint64_t> magicNumbers = {};
    std::vector<uint64_t> masks = {};
    std::vector<int> bitSizes = {};

    int currentTableIndex = 0;

    printf("const uint64_t rookMagicTable[] = {\n");
    for (char y = 0 ; y < 8 ; y++) {
        for (char x = 0 ; x < 8 ; x++) {
            uint64_t mask = rookMasks[x + 8*y];

            int maskSize = std::popcount(mask);
            int possibleCount = 1 << maskSize;

            uint64_t occupencies[possibleCount] = {};
            occupencyPossibilities(mask, occupencies);

            uint64_t attacks[possibleCount] = {};

            for (int i = 0 ; i < possibleCount ; i++) {
                uint64_t occupency = occupencies[i];
                uint64_t attack = 0;
            
                char py = y+1;
                while (py < 8) {
                    uint64_t b = bit(x, py);
                    attack |= b;
                    if (b & occupency) { break; }
                    py += 1;
                }
                py = y-1;
                while (py >= 0) {
                    uint64_t b = bit(x, py);
                    attack |= b;
                    if (b & occupency) { break; }
                    py -= 1;
                }
                char px = x+1;
                while (px < 8) {
                    uint64_t b = bit(px, y);
                    attack |= b;
                    if (b & occupency) { break; }
                    px += 1;
                }
                px = x-1;
                while (px >= 0) {
                    uint64_t b = bit(px, y);
                    attack |= b;
                    if (b & occupency) { break; }
                    px -= 1;
                }
            
                attacks[i] = attack;
            }
        
            // maskSize is perfect hashing, hope we find it
            uint64_t magic = findMagic(occupencies, attacks, maskSize, mask, maskSize);

            int tableSize = 1 << maskSize;
            uint64_t table[tableSize] = {};
            // Fills the table
            testMagic(occupencies, attacks, magic, maskSize, table, maskSize);

            printf("    ");
            for (int i = 0 ; i < tableSize ; i++) {
                printf("0x%llxULL,", table[i]);
            }
            printf("\n");

            ptrIndexes.push_back(currentTableIndex);
            magicNumbers.push_back(magic);
            masks.push_back(mask);
            bitSizes.push_back(maskSize);

            currentTableIndex += tableSize;
        }
    }
    printf("};\n");

    printf("const MagicEntry rookMagicEntries[64] = {\n");
    for (int i = 0 ; i < magicNumbers.size() ; i++) {
        printf(
            "    {&rookMagicTable[%d], 0x%llxULL, 0x%llxULL, %d},\n",
            ptrIndexes[i], magicNumbers[i], masks[i], bitSizes[i]
        );
    }
    printf("};\n");
}

void genBishopMagic() {
    char x = 0;
    char y = 0;

    std::vector<int> ptrIndexes = {};
    std::vector<uint64_t> magicNumbers = {};
    std::vector<uint64_t> masks = {};
    std::vector<int> bitSizes = {};

    int currentTableIndex = 0;

    printf("const uint64_t bishopMagicTable[] = {\n");
    for (char y = 0 ; y < 8 ; y++) {
        for (char x = 0 ; x < 8 ; x++) {
            uint64_t mask = bishopMasks[x + 8*y];

            int maskSize = std::popcount(mask);
            int possibleCount = 1 << maskSize;

            uint64_t occupencies[possibleCount] = {};
            occupencyPossibilities(mask, occupencies);

            uint64_t attacks[possibleCount] = {};

            for (int i = 0 ; i < possibleCount ; i++) {
                uint64_t occupency = occupencies[i];
                uint64_t attack = 0;
            
                char py = y+1;
                char px = x+1;
                while (py < 8 && px < 8) {
                    uint64_t b = bit(px, py);
                    attack |= b;
                    if (b & occupency) { break; }
                    py += 1;
                    px += 1;
                }
                py = y-1;
                px = x+1;
                while (py >= 0 && px < 8) {
                    uint64_t b = bit(px, py);
                    attack |= b;
                    if (b & occupency) { break; }
                    py -= 1;
                    px += 1;
                }
                py = y-1;
                px = x-1;
                while (py >= 0 && px >= 0) {
                    uint64_t b = bit(px, py);
                    attack |= b;
                    if (b & occupency) { break; }
                    py -= 1;
                    px -= 1;
                }
                py = y+1;
                px = x-1;
                while (py < 8 && px >= 0) {
                    uint64_t b = bit(px, py);
                    attack |= b;
                    if (b & occupency) { break; }
                    py += 1;
                    px -= 1;
                }
            
                attacks[i] = attack;
            }
        
            // maskSize is perfect hashing, hope we find it
            uint64_t magic = findMagic(occupencies, attacks, maskSize, mask, maskSize);

            int tableSize = 1 << maskSize;
            uint64_t table[tableSize] = {};
            // Fills the table
            testMagic(occupencies, attacks, magic, maskSize, table, maskSize);

            printf("    ");
            for (int i = 0 ; i < tableSize ; i++) {
                printf("0x%llxULL,", table[i]);
            }
            printf("\n");

            ptrIndexes.push_back(currentTableIndex);
            magicNumbers.push_back(magic);
            masks.push_back(mask);
            bitSizes.push_back(maskSize);

            currentTableIndex += tableSize;
        }
    }
    printf("};\n");

    printf("const MagicEntry bishopMagicEntries[64] = {\n");
    for (int i = 0 ; i < magicNumbers.size() ; i++) {
        printf(
            "    {&bishopMagicTable[%d], 0x%llxULL, 0x%llxULL, %d},\n",
            ptrIndexes[i], magicNumbers[i], masks[i], bitSizes[i]
        );
    }
    printf("};\n");
}

// Should never be called at runtime
void genAllMagic() {
    // ./prog.exe > generatedConstants.cpp
    genBitboardConstants();

    printf(
        "// Changes will be overwritten !\n\n"
        "#include <cstdint>\n\n"
        "struct MagicEntry {\n"
        "   const uint64_t *tablePtr;\n"
        "   const uint64_t magic;\n"
        "   const uint64_t mask;\n"
        "   const int bitSize;\n"
        "};\n\n"
    );

    genRookMagic();
    genBishopMagic();
}


#endif