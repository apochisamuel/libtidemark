#!/bin/bash -eu

cd "$SRC"

# Compile each source file with a unique object name to avoid
# basename collisions (e.g., channel/register.c vs index/register.c)
for src in $(find src -name '*.c'); do
    obj=$(echo "$src" | sed 's|/|_|g; s|\.c$|.o|')
    $CC $CFLAGS -Iinclude -Isrc -c "$src" -o "$obj"
done

ar rcs libtidemark.a *.o

for fuzzer in fuzz/*_fuzzer.c; do
    name=$(basename "$fuzzer" .c)
    $CC $CFLAGS $LIB_FUZZING_ENGINE -Iinclude -Isrc \
        "$fuzzer" libtidemark.a \
        -o "$OUT/$name"
done
