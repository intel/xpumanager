# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
BINNAME=oal
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=
export INCLUDES=-I$(LEVELZERO)/include -I. -I../../hal/core
export CFLAGS=-c -Wall -fPIC
export HEADERS=$(wildcard *.h)
export SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)

all::
	@echo "Compiling..."

all:: $(SOURCES) $(BINNAME)

all::
	@echo "Done"

$(BINNAME): $(OBJECTS)
	@if [ "$(DBG_FLAGS)" = "" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

.cpp.o:
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $<

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f $(BINNAME) *.o

