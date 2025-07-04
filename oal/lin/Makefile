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
export CFLAGS=-c -Wall -fPIC -std=c++20
export SOURCES := $(wildcard $(SRCDIR)/*.cpp)
export HEADERS := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Check if VERBOSE is set to 1, if not, silence the commands
ifeq ($(VERBOSE),1)
CMD_PREFIX=
else
CMD_PREFIX=@
endif

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
	$(CMD_PREFIX)$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:
	@export DBG_FLAGS='-g -D _DEBUG'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	$(CMD_PREFIX)rm -f $(BINNAME) $(OBJECTS)
	$(CMD_PREFIX)rm -rf $(OBJDIR)

