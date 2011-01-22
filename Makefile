CC := gcc
CXX := g++
CFLAGS := -O3 -g -pthread -Wall -march=native -msse -msse2 -mfpmath=sse -std=c99
CXXFLAGS := -O3 -g -pthread -Wno-unused-result -Wall -ffast-math -march=native -msse -msse2 -mfpmath=sse
LDFLAGS := -lm -pthread -ffast-math

SRCS := evolution.C poi.C image.c config.C gui.C gui-gtk.c \
		config-example.C gui-example.C

image.o: CFLAGS += $(shell pkg-config --cflags gdk-pixbuf-2.0)
gui-gtk.o: CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
gui.o: CXXFLAGS += $(shell pkg-config --cflags cairo)
image-example: LDFLAGS += $(shell pkg-config --libs gdk-pixbuf-2.0)
evolution gui-example: LDFLAGS += $(shell pkg-config --libs gtk+-2.0 cairo)

evolution: evolution.o poi.o gui.o gui-gtk.o workers.o image.o config.o
gui-example: gui.o gui-gtk.o gui-example.o image.o workers.o
config-example: config-example.o config.o
image-example: image-example.o image.o

%: %.o
	$(CXX) $(LDFLAGS) $^ -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
