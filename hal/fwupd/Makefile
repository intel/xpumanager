# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export INCLUDES=-I. -I../../oal/lin -I../core -I$(LEVELZERO)/include -I$(IGSC)/include
export CFLAGS=-c -Wall -fPIC
SRCDIR := .
OBJDIR := obj
export SOURCES := $(wildcard $(SRCDIR)/*.cpp)
export HEADERS := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

all::
	@echo "Compiling..."

all::$(OBJECTS)

all::
	@echo "Done"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@echo $<
	@mkdir -p $(OBJDIR)
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -rf $(OBJDIR)

