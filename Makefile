CC := gcc
CXX := g++
CFLAGS := -O3 -g -pthread -Wall -march=native -msse -msse2 -mfpmath=sse -std=c99
CXXFLAGS := -O3 -g -pthread -Wno-unused-result -Wall -ffast-math -march=native -msse -msse2 -mfpmath=sse
CFLAGS_gui-gtk := $(shell pkg-config --cflags gtk+-2.0 cairo)
CXXFLAGS_image := $(shell pkg-config --cflags cairo)
LDFLAGS := -lm -pthread -ffast-math

SRCS := evolution.C poi.C image.C config.C gui.C gui-gtk.c \
		config-example.C gui-example.C 

evolution: evolution.o poi.o gui.o gui-gtk.o workers.o image.o config.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0 cairo` $^ -o $@
poi: poi.o image.o gui.o gui-gtk.o workers.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0 cairo` $^ -o $@

gui-example: gui.o gui-gtk.o gui-example.o image.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0 cairo` $^ -o $@
config-example: config-example.o config.o
	$(CXX) $(LDFLAGS)  $^ -o $@
	
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) ${CFLAGS_$(basename $@)} -c $< -o $@
%.o: %.C
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ${CXXFLAGS_$(basename $@)} -c $< -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
