CC := gcc
CXX := g++
CFLAGS := -O3 -g -pthread -Wall -march=native -msse -msse2 -mfpmath=sse -std=c99
CXXFLAGS := -O3 -g -pthread -Wno-unused-result -Wall -ffast-math -march=native -msse -msse2 -mfpmath=sse
CFLAGS_gui-gtk := $(shell pkg-config --cflags gtk+-2.0 cairo)
CXXFLAGS_composite := $(shell pkg-config --cflags cairo)
LDFLAGS := -lm -pthread -ffast-math

SRCS := evolution.C poi.C image.C composite.C config.C gui.C gui-gtk.c \
		config-example.C gui-example.C composite-test.C

evolution: evolution.o poi.o composite.o gui.o gui-gtk.o workers.o image.o config.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0 cairo` $^ -o $@
poi: poi.o image.o composite.o gui.o gui-gtk.o workers.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0 cairo` $^ -o $@

gui-example: gui.o composite.o gui-gtk.o gui-example.o image.o workers.o
	$(CXX) $(LDFLAGS) `pkg-config --libs gtk+-2.0 cairo` $^ -o $@
config-example: config-example.o config.o
	$(CXX) $(LDFLAGS)  $^ -o $@
composite-test: composite-test.o composite.o image.o workers.o
	$(CXX) $(LDFLAGS)  `pkg-config --libs cairo` $^ -o $@
	
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) ${CFLAGS_$(basename $@)} -c $< -o $@
%.o: %.C
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ${CXXFLAGS_$(basename $@)} -c $< -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
