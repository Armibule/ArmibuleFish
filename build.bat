:: g++ main.cpp -I src/include -L src/lib -o prog.exe -l mingw32 -l SDL2main -l SDL2 -l SDL2_image

:: Use MSys Mingw x64
:: g++ main.cpp -o prog.exe -I/mingw64/include -L/mingw64/lib -lSDL2main -lSDL2 -lSDL2_image

cd "D:\Documents\Documents Laurent & Valérie\Mes docs Armand\créations\C++\chess_3"

g++ main.cpp -o prog.exe -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -O3 -std=c++20 -m64 -mlzcnt


.\prog
