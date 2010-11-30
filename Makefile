CC := gcc
CXX := g++
CFLAGS := -O2 -Wall -std=c99
CXXFLAGS := -O2 -Wall 
CFLAGS_gui-gtk := $(shell pkg-config --cflags gtk+-2.0)

SRCS := image.C gui-example.C gui-gtk.c findCorners.C

all:

findCorners: findCorners.o image.o

gui-example: gui-gtk.o gui-example.o image.o
	$(CXX) `pkg-config --libs gtk+-2.0` gui-gtk.o gui-example.o image.o -o gui-example

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) ${CFLAGS_$(basename $@)} -c $< -o $@
%.o: %.C
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ${CXXFLAGS_$(basename $@)} -c $< -o $@

Makefile.deps: $(SRCS)
	$(CC) -MM $^ > $@
include Makefile.deps
