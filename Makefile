CC := gcc
CXX := g++
CFLAGS := -O3 -g -pthread -Wall -march=native -msse -msse2 -mfpmath=sse -std=c99 $(shell pkg-config --cflags cairo)
CXXFLAGS := -O3 -g -pthread -Wno-unused-result -Wall -ffast-math -march=native -msse -msse2 -mfpmath=sse $(shell pkg-config --cflags cairo)
LDFLAGS := -lm -pthread -ffast-math $(shell pkg-config --libs cairo gdk-pixbuf-2.0)

SRCS := evolution.C evosingle.C \
	    poi.C image.c config.C gui.C gui-gtk.c util.C \
		poi-test.C

image.o: CFLAGS += $(shell pkg-config --cflags gdk-pixbuf-2.0)
gui-gtk.o: CFLAGS += $(shell pkg-config --cflags gtk+-2.0)
evolution evosingle poi-test gui-example: LDFLAGS += $(shell pkg-config --libs gtk+-2.0)

all: evolution evosingle
evolution: evolution.o poi.o gui.o gui-gtk.o util.o image.o config.o
evosingle: evosingle.o poi.o gui.o gui-gtk.o util.o image.o config.o
poi-test: poi-test.o poi.o image.o util.o gui.o gui-gtk.o

%: %.o
	$(CXX) $(LDFLAGS) $^ -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
