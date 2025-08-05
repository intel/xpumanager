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
export INCLUDES=-I. -I$(OALDIR) -I$(HALCORE) -I$(FWDIR) -I$(LEVELZERO)/include -I$(IGSC)/include -I../third_party
export CFLAGS=-c -Wall -Wextra -Werror -pedantic -std=c++20
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
	@export DBG_FLAGS='-g -D _DEBUG'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	$(CMD_PREFIX)rm -rf $(OBJDIR)

