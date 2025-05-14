# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
SMINAME=xpu-smi
CLINAME=xpu-cli
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L../../hal/core -lxpum -L$(LEVELZERO)/build/lib -lze_loader -L$(IGSC)/build/lib -ligsc
export INCLUDES=-I. -I../cmn -I../../oal/lin -I../../hal/core -I../../hal/fwupd -I$(LEVELZERO)/include -I$(IGSC)/include
export CFLAGS=-c -Wall
export HEADERS=$(wildcard *.h)
export SOURCES=$(wildcard *.cpp)
export DEFINES_DMN = -DDAEMONMODE
OBJECTS_SMI=$(SOURCES:.cpp=.smi.o)
OBJECTS_CLI=$(SOURCES:.cpp=.cli.o)
OTHER_OBJECTS = $(wildcard ../cmn/*.o)
OAL_OBJECTS = $(wildcard ../../oal/lin/*.o)

all::
	@echo "Compiling..."

all::$(OAL_OBJECTS) $(SOURCES) $(SMINAME) $(CLINAME) $(OTHER_OBJECTS)

all::
	@echo "Done"

$(SMINAME):  $(OAL_OBJECTS) $(OBJECTS_SMI) $(OTHER_OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@  $(OAL_OBJECTS) $(OBJECTS_SMI) $(OTHER_OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

$(CLINAME):  $(OAL_OBJECTS) $(OBJECTS_CLI) $(OTHER_OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@  $(OAL_OBJECTS) $(OBJECTS_CLI) $(OTHER_OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

%.smi.o: %.cpp $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $< -o $@

%.cli.o: %.cpp $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $(DEFINES_DMN) $< -o $@

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f $(SMINAME) $(CLINAME) *.o

