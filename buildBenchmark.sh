g++ main.cpp -o prog.exe -no-pie -g -pg -Og -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -std=c++20 -m64 -mlzcnt

./prog.exe

gprof prog.exe gmon.out > perfResults/analyse1.txt

gprof -l prog.exe gmon.out > perfResults/analyseLigne1.txt




g++ main.cpp -o prog.exe -no-pie -g -pg -O3 -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -std=c++20 -m64 -mlzcnt

./prog.exe

gprof prog.exe gmon.out > perfResults/analyse1-O3.txt

gprof -l prog.exe gmon.out > perfResults/analyseLigne1-O3.txt
