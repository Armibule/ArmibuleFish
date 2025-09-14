:: Use MSys Mingw x64

g++ testing/testStrength.cpp -o testing/test.exe -O3 -std=c++20 -m64 -mlzcnt

./testing/test

./testing/test 10
