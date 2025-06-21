# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

export PWD=$(shell pwd)
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export INCLUDES=-I. -I../../oal/lin -I../core -I$(LEVELZERO)/include -I$(IGSC)/include
export CFLAGS=-c -Wall -fPIC -std=c++20
SRCDIR := .
OBJDIR := obj
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

all::$(OBJECTS)

all::
	@echo "Done"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@echo $<
	@mkdir -p $(OBJDIR)
	$(CMD_PREFIX)$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:
	$(CMD_PREFIX)export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	$(CMD_PREFIX)rm -rf $(OBJDIR)

