CC ?= cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -I.
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
.PHONY: all test clean
all: tests/test_steam tests/libmocksteam.so
tests/libmocksteam.so: tests/mock_steam.c tests/mock_control.h steam_abi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared tests/mock_steam.c -o $@
tests/test_steam: tests/test_steam.c steam.c steam_load.c steam.h steam_abi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_steam.c steam.c steam_load.c -ldl -o $@
test: all
	./tests/test_steam
clean:
	rm -f tests/test_steam tests/libmocksteam.so
