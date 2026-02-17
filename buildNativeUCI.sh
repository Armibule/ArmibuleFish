# Use MSys Mingw x64
# Fastest build for your architecture, UCI mode

g++ UCIMain.cpp -o progNativeUCI.exe -Ofast -std=c++23 -march=native -I/mingw64/include -L/mingw64/lib -lmingw32 -m64 -mlzcnt --static


./progNativeUCI.exe
