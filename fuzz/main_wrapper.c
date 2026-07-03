#include <stdio.h>
#include <stdlib.h>

extern int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <input>\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc(fsize);
    if (!buf) return 1;
    if (fread(buf, 1, fsize, f) != (size_t)fsize) { free(buf); fclose(f); return 1; }
    fclose(f);
    LLVMFuzzerTestOneInput(buf, fsize);
    free(buf);
    return 0;
}
