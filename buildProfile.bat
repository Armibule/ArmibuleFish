cd "D:\Documents\Documents Laurent & Valérie\Mes docs Armand\créations\C++\chess_3"

g++ main.cpp -no-pie -g -pg -o prog.exe -I/mingw64/include -L/mingw64/lib -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -std=c++20 -m64 -mlzcnt

./prog

gprof prog.exe gmon.out > perfResults/analyse2.txt

gprof -l prog.exe gmon.out > perfResults/analyseLigne2.txt





g++ main.cpp -no-pie -g -pg -o prog.exe -I/mingw64/include -L/mingw64/lib -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -std=c++20 -m64 -mlzcnt -O3

./prog

gprof prog.exe gmon.out > perfResults/analyse2-O3.txt

gprof -l prog.exe gmon.out > perfResults/analyseLigne2-O3.txt
