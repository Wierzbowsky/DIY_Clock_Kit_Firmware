sdcc -o build/ src/main.c --disable-warning 94 --disable-warning 158 --disable-warning 283
copy build\main.ihx flash\main.hex