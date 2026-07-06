
# Tests if version 2 is better than version 1


# Very fast test

fastchess \
    -engine cmd=progNativeUCI_2.exe name=Version2 \
    -engine cmd=progNativeUCI_1.exe name=Version1 \
    -openings file=testing/UHO_Lichess_4852_v1.epd format=epd order=random \
    -each tc=4.0+0.04 \
    -rounds 40 -repeat -concurrency 4


# Fast test

fastchess \
    -engine cmd=progNativeUCI_2.exe name=Version2 \
    -engine cmd=progNativeUCI_1.exe name=Version1 \
    -openings file=testing/UHO_Lichess_4852_v1.epd format=epd order=random \
    -each tc=8.0+0.08 \
    -rounds 50 -repeat -concurrency 4


# Debug

fastchess \
    -engine cmd=progNativeUCI_2.exe name=Version2 \
    -engine cmd=progNativeUCI_1.exe name=Version1 \
    -openings file=testing/UHO_Lichess_4852_v1.epd format=epd order=random \
    -each tc=5.0+0.1 \
    -rounds 5 -repeat -concurrency 1 \
    -log file=testing/testLog.log engine=true


# Against older

fastchess \
    -engine cmd=progNativeUCI_1.exe name=ArmibuleFishV9 \
    -engine cmd=progNativeUCI_v8.exe name=ArmibuleFishV8 \
    -openings file=testing/UHO_Lichess_4852_v1.epd format=epd order=random \
    -each tc=8.0+0.08 \
    -rounds 40 -repeat -concurrency 3
