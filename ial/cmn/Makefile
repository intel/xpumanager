# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L../../hal -lxpum -lpciaccess
export INCLUDES=-I. -I../../oal/lin -I../../hal -I$(LEVELZERO)/include
export CFLAGS=-c -Wall
export HEADERS=$(wildcard *.h) 
export SOURCES=$(wildcard *.cpp) 
OBJECTS=$(SOURCES:.cpp=.o)

all::
	@echo "Compiling..."

all::$(OBJECTS)

all::
	@echo "Done"

%.o: %.cpp $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $< 

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f *.o

