:: Use MSys Mingw x64
g++ main.cpp -o prog.exe -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -O3 -std=c++20 -m64 -mlzcnt

.\prog
