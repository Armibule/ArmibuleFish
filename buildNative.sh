# Use MSys Mingw x64
# Fastest build for your architecture

g++ main.cpp -o progNative.exe -Ofast -std=c++23 -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt


./progNative.exe
./progNative.exe  --white
./progNative.exe  --black
./progNative.exe  --help



# Static linking ?

# Linker flags :
# pkg-config --libs --static SDL2
# pkg-config --libs --static SDL2_ttf
# pkg-config --libs --static SDL2_Image
# pkg-config --libs --static SDL2_gfx

g++ main.cpp -o progNativeStatic.exe -Ofast -std=c++23 -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt \
    -lmingw32 -mwindows -lSDL2main -lSDL2 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8 \
    -lSDL2_ttf -lmingw32 -mwindows -lSDL2main -lSDL2 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8 -lfreetype -lbz2 -lpng16 -lz -lharfbuzz -lm -lusp10 -lgdi32 -lrpcrt4 -luser32 -ldwrite -lglib-2.0 -lintl -lws2_32 -lole32 -lwinmm -lshlwapi -luuid -latomic -lm -lpcre2-8 -lgraphite2 -lbrotlidec -lbrotlicommon \
    -lSDL2_image -lmingw32 -mwindows -lSDL2main -lSDL2 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8 -lpng16 -lz -ljxl -lm -lstdc++ -lhwy -lbrotlienc -lbrotlidec -lbrotlicommon -ljxl_cms -lm -llcms2_fast_float -lm -pthread -ltiff -ljbig -lzstd -llzma -lLerc -ljpeg -ldeflate -lz -lavif -lyuv -ldav1d -lrav1e -lkernel32 -lntdll -luserenv -lws2_32 -ldbghelp -lSvtAv1Enc -laom -lwebpdemux -lwebp -lsharpyuv \
    -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8




# With profile data


g++ -fprofile-use main.cpp -o progNative.exe -Ofast -std=c++23 -march=native -I/mingw64/include -L/mingw64/lib -lSDL2_gfx -lmingw32 -mwindows -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -m64 -mlzcnt
