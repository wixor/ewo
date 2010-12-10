CC := gcc
CXX := g++
CFLAGS := -O2 -pthread -Wall -march=native -msse -msse2 -mfpmath=sse -std=c99
CXXFLAGS := -O2 -pthread -Wno-unused-result -Wall -ffast-math -march=native -msse -msse2 -mfpmath=sse
CFLAGS_gui-gtk := $(shell pkg-config --cflags gtk+-2.0)
LDFLAGS := -lm -pthread -ffast-math

SRCS := image.C config-example.C config.C gui-example.C gui-gtk.c \
        poi.C evolution.C

all:

poi: poi.o image.o gui-gtk.o workers.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0` $^ -o $@

gui-example: gui-gtk.o gui-example.o image.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0` $^ -o $@
config-example: config-example.o config.o
	$(CXX) $(LDFLAGS)  $^ -o $@

evolution: evolution.o gui-gtk.o workers.o image.o poi.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0` $^ -o $@
	
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) ${CFLAGS_$(basename $@)} -c $< -o $@
%.o: %.C
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ${CXXFLAGS_$(basename $@)} -c $< -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
