# For extra performances, generates profile data for later
# Does not work for now, mingw bug on windows ?

g++ -fprofile-generate main.cpp -o progProfile.exe -Ofast -std=c++20 -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt

./progProfile
