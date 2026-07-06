# Use MSys Mingw x64
# Fastest build for your architecture

g++ main.cpp -o progNative.exe -Ofast -std=c++23 -march=native -freorder-blocks-and-partition -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt



./progNative
./progNative  --white
./progNative  --black
./progNative  --help

# Test pos
./progNative -fen "4r1k1/1p2qpb1/p1p4p/P2rN3/1PQPpP2/4P3/6KP/1RR5 b - - 0 37"

# Example
./progNative --white -time 10000 30000


# Profile Guided Optimisations (broken)
# Test with profile generate
g++ main.cpp -o progNative.exe -fprofile-generate -Ofast -std=c++23 -march=native -freorder-blocks-and-partition -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt

# Use profile data
g++ -fprofile-use -fprofile-correction main.cpp -o progNative.exe -Ofast -std=c++23 -march=native -freorder-blocks-and-partition -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt
