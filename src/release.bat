set VER=013
del wmagnify%VER%.zip
strip wmagnify.exe
7z a -tzip wmagnify%VER%.zip wmagnify.exe
7z a -tzip wmagnify%VER%src.zip wmagnify.c wmagnify.ico wmagnifr.rc make.bat
