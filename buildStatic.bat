:: pkg-config --libs --static SDL2 SDL2_Image SDL2_ttf

g++ main.cpp -o progStatic.exe -I/mingw64/include -L/mingw64/lib -static -std=c++20 -m64 -mlzcnt -O3 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lset upapi -lshell32 -ldinput8 -lSDL2_image -ljxl -lm -lstdc++ -lhwy -lbrotlienc -ljxl_cms -lm -lstdc++ - llcms2 -llcms2_fast_float -lm -pthread -ltiff -ljbig -lz -lzstd -llzma -lLerc -ljpeg -ldeflate -lavi f -lyuv -ldav1d -lrav1e -lkernel32 -ladvapi32 -lntdll -luserenv -lws2_32 -ldbghelp -lSvtAv1Enc -laom  -lwebpdemux -lwebp -lsharpyuv -lSDL2_ttf -lmingw32 -mwindows -lSDL2main -lSDL2 -lm -lkernel32 -luse r32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldi nput8 -lfreetype -lbz2 -lpng16 -lz -lharfbuzz -lm -lusp10 -lgdi32 -lrpcrt4 -luser32 -ldwrite -lglib- 2.0 -lintl -lws2_32 -lole32 -lwinmm -lshlwapi -luuid -latomic -lm -lpcre2-8 -lgraphite2 -lbrotlidec -lbrotlicommon
:: error ??
:: g++.exe: error: -E or -x required when input is from standard input

