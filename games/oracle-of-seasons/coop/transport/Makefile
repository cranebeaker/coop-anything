CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -I.
.PHONY: all test clean
all: tests/test_net tests/test_short_write
tests/test_net: tests/test_net.c net.c net.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_net.c net.c -o $@
tests/test_short_write: tests/test_short_write.c net.c net.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_short_write.c net.c -Wl,--wrap=send -o $@
test: tests/test_net tests/test_short_write
	./tests/test_net
	./tests/test_short_write
clean:
	rm -f tests/test_net tests/test_short_write
