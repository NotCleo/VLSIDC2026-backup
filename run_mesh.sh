SRC="mesh_overlay.cpp"
EXE="mesh_overlay"

echo "Compiling $SRC..."
g++ -O3 -std=c++17 $SRC -o $EXE `pkg-config --cflags --libs opencv4`

if [ $? -eq 0 ]; then
    echo "Compilation done"
    ./$EXE
else
    echo "Compilation failed!"
    exit 1
fi
