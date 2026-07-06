g++ main.cpp -o progBench.exe -no-pie -g -pg -Og -ffast-math -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -std=c++20 -m64 -mlzcnt

./progBench.exe

gprof progBench.exe gmon.out > perfResults/analyse2.txt

gprof -l progBench.exe gmon.out > perfResults/analyseLigne2.txt




g++ main.cpp -o progBench.exe -no-pie -g -pg -fno-omit-frame-pointer -Ofast -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -std=c++20 -m64 -mlzcnt

./progBench.exe

gprof progBench.exe gmon.out > perfResults/analyse1-Ofast.txt

gprof -l progBench.exe gmon.out > perfResults/analyseLigne1-Ofast.txt
