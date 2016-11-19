SHELL = /bin/sh
CXX = g++
INSTALL = install -c
INSTALLDATA = instal -c -m 644
LIBS = 
CFLAGS = -g -O2 -Wall -I.
CPPFLAGS = $(CFLAGS)
LDFLAGS = -g
prefix = $(HOME)
bindir_relative = bin
bindir = $(prefix)/bin
SRCS_CPP = client.cpp
SRCS = $(SRCS_CPP)
OBJS = $(pathsubst %.c,%.o,$(SCRS))
AUX = README.md LICENSE Makefile
RM = rm -f

.PHONY: all
all: ibrc.pdf ibrcc

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^

.PHONY: install
install: all
	$(INSTALL) ibrcc $(bindir)/$(binprefix)ibrc

.PHONY: clean
clean:
	$(RM) *.o *.aux *.log *.out ibrcc ibrc.pdf

ibrc.pdf: doc/ibrc.tex
	pdflatex $^

ibrcc: client.o data.hpp
	$(CXX) $(CXXFLAGS) -o $@ $^
