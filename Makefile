CC := gcc
CXX := g++
CFLAGS := -O3 -g -pthread -Wall -march=native -msse -msse2 -mfpmath=sse -std=c99 $(shell pkg-config --cflags cairo)
CXXFLAGS := -O3 -g -pthread -Wno-unused-result -Wall -ffast-math -march=native -msse -msse2 -mfpmath=sse $(shell pkg-config --cflags cairo)
LDFLAGS := -lm -pthread -ffast-math $(shell pkg-config --libs cairo)

SRCS := evolution.C poi.C image.c config.C gui.C gui-gtk.c util.C \
		config-example.C gui-example.C

image.o: CFLAGS += $(shell pkg-config --cflags gdk-pixbuf-2.0)
gui-gtk.o: CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
image-example: LDFLAGS += $(shell pkg-config --libs gdk-pixbuf-2.0)
evolution gui-example: LDFLAGS += $(shell pkg-config --libs gtk+-2.0)

evolution: evolution.o poi.o gui.o gui-gtk.o util.o image.o config.o
gui-example: gui.o gui-gtk.o gui-example.o image.o util.o poi.o
config-example: config-example.o config.o
image-example: image-example.o image.o

%: %.o
	$(CXX) $(LDFLAGS) $^ -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
