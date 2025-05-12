# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
BINNAME=xpu-smi
DMNNAME=daemon
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L../../hal/core -lxpum -L$(LEVELZERO)/build/lib -lze_loader -L$(IGSC)/build/lib -ligsc
export INCLUDES=-I. -I../cmn -I../../oal/lin -I../../hal/core -I../../hal/fwupd -I$(LEVELZERO)/include -I$(IGSC)/include
export CFLAGS=-c -Wall
export HEADERS=$(wildcard *.h)
export SOURCES=$(wildcard *.cpp)
export DEFINES_DMN = -DDAEMONMODE
OBJECTS_BIN=$(SOURCES:.cpp=.bin.o)
OBJECTS_DMN=$(SOURCES:.cpp=.dmn.o)
OTHER_OBJECTS = $(wildcard ../cmn/*.o)
OAL_OBJECTS = $(wildcard ../../oal/lin/*.o)

all::
	@echo "Compiling..."

all::$(OAL_OBJECTS) $(SOURCES) $(BINNAME) $(DMNNAME) $(OTHER_OBJECTS)

all::
	@echo "Done"

$(BINNAME):  $(OAL_OBJECTS) $(OBJECTS_BIN) $(OTHER_OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@  $(OAL_OBJECTS) $(OBJECTS_BIN) $(OTHER_OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

$(DMNNAME):  $(OAL_OBJECTS) $(OBJECTS_DMN) $(OTHER_OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@  $(OAL_OBJECTS) $(OBJECTS_DMN) $(OTHER_OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

%.bin.o: %.cpp $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $< -o $@

%.dmn.o: %.cpp $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $(DEFINES_DMN) $< -o $@

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f $(BINNAME) $(DMNNAME) *.o

