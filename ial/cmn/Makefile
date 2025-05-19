# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

# Directories
SRCDIR := .
OBJDIR := obj
OALDIR := ../../oal/lin
HALCORE := ../../hal/core
FWDIR  := ../../hal/fwupd

export PWD=$(shell pwd)
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L$(HALCORE) -lxpum -lpciaccess
export INCLUDES=-I. -I$(OALDIR) -I$(HALCORE) -I$(FWDIR) -I$(LEVELZERO)/include -I$(IGSC)/include
export CFLAGS=-c -Wall
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

