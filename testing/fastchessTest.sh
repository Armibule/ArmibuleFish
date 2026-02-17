
# Tests if version 2 is better than version 1


# Fast test

"D:\Documents\Documents Laurent & Valérie\Mes docs Armand\créations\C++\fastchess\fastchess.exe" \
    -engine cmd=progNativeUCI_2.exe name=Version2 \
    -engine cmd=progNativeUCI_1.exe name=Version1 \
    -openings file=testing/UHO_Lichess_4852_v1.epd format=epd order=random \
    -each tc=5.0+0.1 \
    -rounds 20 -repeat -concurrency 2


# Debug

"D:\Documents\Documents Laurent & Valérie\Mes docs Armand\créations\C++\fastchess\fastchess.exe" \
    -engine cmd=progNativeUCI_2.exe name=Version2 \
    -engine cmd=progNativeUCI_1.exe name=Version1 \
    -openings file=testing/UHO_Lichess_4852_v1.epd format=epd order=random \
    -each tc=5.0+0.1 \
    -rounds 5 -repeat -concurrency 1 \
    -log file=testing/testLog.log engine=true



