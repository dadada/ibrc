.PHONY: all clean doc client debug

SHELL = /bin/sh
CC = g++
CXX_FLAGS = -Wall
CXX_DEBUG_FLAGS = -g
export


all: client doc

clean:
	$(MAKE) -wC src/client clean
	$(MAKE) -wC doc clean

debug: CXXFLAGS += $(CXX_DEBUG_FLAGS)

doc:
	$(MAKE) -wC doc all

client:
	$(MAKE) -wC src/client all

