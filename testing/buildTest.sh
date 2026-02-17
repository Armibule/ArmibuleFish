:: Use MSys Mingw x64

g++ testing/testStrength.cpp -o testing/test.exe -Ofast -std=c++23 -march=native -m64 -mlzcnt

./testing/test

./testing/test 10
