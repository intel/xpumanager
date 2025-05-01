# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
BINNAME=xpu-smi
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L../../hal -lxpum
export INCLUDES=-I. -I../cmn -I../../oal/lin -I../../hal -I$(LEVELZERO)/include
export CFLAGS=-c -Wall
export HEADERS=$(wildcard *.h)
export SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
OTHER_OBJECTS = $(wildcard ../cmn/*.o)

all::
	@echo "Compiling..."

all::$(SOURCES) $(BINNAME) $(OTHER_OBJECTS)

all::
	@echo "Done"

$(BINNAME): $(OBJECTS) $(OTHER_OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@ $(OBJECTS) $(OTHER_OBJECTS) $(LIBS)
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

