SHELL = /bin/sh
CXX = g++
INSTALL = install
RM = rm -f
CFLAGS = -Wall \
	 -Wextra -Wcast-align -Wcast-qual -Wpointer-arith \
	 -Wunreachable-code -Wfloat-equal \
	 -Wformat=2 -Wredundant-decls -Wundef \
	 -Wdisabled-optimization -Wshadow -Wmissing-braces \
	 -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wconversion \
	 -Wno-unused-parameter \
	 -pedantic -std=c++11
CFLAGS_DEBUG = -g3 -O0 -DDEBUG
CFLAGS_RELEASE = -O2 -march=native \
		 -mtune=native \
		 -ftree-vectorize \
		 -fstack-protector \
		 -D_FORTIFY_SOURCE=2
prefix = $(HOME)
bindir = $(prefix)/bin
SRCS = client.cpp data.cpp helpers.cpp server.cpp
OBJS = $(patsubst %.cpp,%.o,$(SRCS))
BIN = ibrcc ibrcd
DOC = ibrc.pdf
AUX = README.md LICENSE Makefile

all: debug

debug: CFLAGS += $(CFLAGS_DEBUG)
debug: $(BIN)

release: CFLAGS += $(CFLAGS_RELEASE)
release: $(BIN) doc

doc: ibrc.pdf

install: release
	$(INSTALL) ibrcc $(bindir)/$(binprefix)ibrcc

clean:
	$(RM) $(OBJS) $(BIN) $(patsubst %.pdf,%.aux,$(DOC)) $(patsubst %.pdf,%.log,$(DOC)) $(patsubst %.pdf,%.out,$(DOC)) $(DOC)

ibrc.pdf: doc/ibrc.tex
	pdflatex $^

ibrcc: client.o data.o helpers.o
	$(CXX) $(CFLAGS) -o $@ $(LDFLAGS) $(filter %.o,$^) $(LIBS)

ibrcd: server.o data.o helpers.o
	$(CXX) $(CFLAGS) -o $@ $(LDFLAGS) $(filter %.o,$^) $(LIBS)

%.o: %.cpp $(DEPS)
	$(CXX) -c $< $(CFLAGS)

.PHONY: all clean install debug release doc
