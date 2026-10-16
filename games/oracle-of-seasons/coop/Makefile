CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -I.
LDLIBS += -lm

.PHONY: all test preview clean
all: build/test_marker build/test_marker_draw
build/test_marker: marker.c marker.h tests/test_marker.c
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) marker.c tests/test_marker.c $(LDLIBS) -o $@
build/test_marker_draw: marker_draw.c marker_draw.h marker.h tests/test_marker_draw.c
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) marker_draw.c tests/test_marker_draw.c $(LDLIBS) -o $@
test: build/test_marker build/test_marker_draw
	./build/test_marker
	./build/test_marker_draw
build/preview_marker: marker.c marker.h marker_draw.c marker_draw.h tests/preview_marker.c
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) marker.c marker_draw.c tests/preview_marker.c $(LDLIBS) -o $@
preview: build/preview_marker
	./build/preview_marker build/marker-contact-sheet.ppm
clean:
	rm -rf build
