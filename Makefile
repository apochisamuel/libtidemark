CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare -std=c11 -Iinclude -Isrc
LDFLAGS = -lm

# Source files
SRCS = $(shell find src -name '*.c')
OBJS = $(SRCS:.c=.o)

# Library
LIB = libtidemark.a

.PHONY: all clean fuzz asan

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ASAN build for fuzz harnesses
ASAN_CC = gcc
ASAN_CFLAGS = -fsanitize=address -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare -fno-omit-frame-pointer -g -O1 \
              -Wall -Wextra -std=c11 -Iinclude -Isrc
ASAN_LDFLAGS = -lm -fsanitize=address

asan: clean
	$(MAKE) CC="$(ASAN_CC)" CFLAGS="$(ASAN_CFLAGS)" LDFLAGS="$(ASAN_LDFLAGS)" $(LIB)

fuzz-stream: asan
	$(ASAN_CC) $(ASAN_CFLAGS) fuzz/stream_fuzzer.c -o fuzz/stream_fuzzer $(LIB) $(ASAN_LDFLAGS)

fuzz-channel: asan
	$(ASAN_CC) $(ASAN_CFLAGS) fuzz/channel_fuzzer.c -o fuzz/channel_fuzzer $(LIB) $(ASAN_LDFLAGS)

fuzz-session: asan
	$(ASAN_CC) $(ASAN_CFLAGS) fuzz/session_fuzzer.c -o fuzz/session_fuzzer $(LIB) $(ASAN_LDFLAGS)

fuzz-migrate: asan
	$(ASAN_CC) $(ASAN_CFLAGS) fuzz/migrate_fuzzer.c -o fuzz/migrate_fuzzer $(LIB) $(ASAN_LDFLAGS)

clean:
	find . -name '*.o' -delete
	rm -f $(LIB) fuzz/*_fuzzer fuzz/*.bin
