wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h

riscv64-linux-gnu-gcc -static capture-final.c -o capture_tool -lm

