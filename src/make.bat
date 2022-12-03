@rem windres -o wmagnifr.o wmagnifr.rc
@rem gcc -Wall -fomit-frame-pointer -O3 -o wmagnify.exe wmagnifr.o wmagnify.c -mwindows -Wno-misleading-indentation && wmagnify -t 25x -T
gcc -Wall -fomit-frame-pointer -O3 -o wmagnify.exe wmagnifr.o wmagnify.c -mwindows  && wmagnify -t 25x -T