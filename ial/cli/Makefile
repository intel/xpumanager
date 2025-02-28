# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
BINNAME=xpu-smi
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L../../hal -lxpum -lpciaccess
export INCLUDES=-I. -I../../hal -I../../oal/lin
export CFLAGS=-c -Wall
export HEADERS=$(wildcard *.h)
export SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)

all::
	@echo "Compiling..."

all::$(SOURCES) $(BINNAME)

all::
	@echo "Done"

$(BINNAME): $(OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@ $(OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

%.o: %.cpp $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $< 

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f $(BINNAME) *.o

