# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

# Directories
SRCDIR := .
OBJDIR := obj
HALCORE := ../../hal/core

export PWD=$(shell pwd)
BINNAME=oal
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=
export INCLUDES=-I$(LEVELZERO)/include -I. -I$(HALCORE)
export CFLAGS=-c -Wall -fPIC
export SOURCES := $(wildcard $(SRCDIR)/*.cpp)
export HEADERS := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

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

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@echo $<
	@mkdir -p $(OBJDIR)
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f $(BINNAME) $(OBJECTS)
	@rm -rf $(OBJDIR)

