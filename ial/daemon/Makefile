# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

# Directories
SRCDIR := .
OBJDIR := obj
CMNDIR := ../cmn
OALROOTDIR := ../../oal
OALDIR := ../../oal/lin
HALCORE := ../../hal/core
FWDIR  := ../../hal/fwupd

export PWD=$(shell pwd)
DAEMONNAME=xpumd
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L$(HALCORE) -lxpum -L$(LEVELZERO)/build/lib -lze_loader -L$(IGSC)/build/lib -ligsc -lhwloc -lcurl
export INCLUDES=-I. -I$(CMNDIR) -I$(OALROOTDIR) -I$(OALDIR) -I$(HALCORE) -I$(FWDIR) -I$(LEVELZERO)/include -I$(IGSC)/include -I../third_party
export CFLAGS=-c -Wall -Wextra -Werror -pedantic -std=c++20
export SOURCES := $(wildcard $(SRCDIR)/*.cpp)
export HEADERS := $(wildcard $(SRCDIR)/*.h)
export DEFINES_DMN = -DDAEMONMODE
OBJECTS_DAEMON := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.dmn.o)
OTHER_OBJECTS = $(wildcard $(CMNDIR)/$(OBJDIR)/*.o)
OAL_OBJECTS = $(wildcard $(OALDIR)/$(OBJDIR)/*.o)

# Check if VERBOSE is set to 1, if not, silence the commands
ifeq ($(VERBOSE),1)
CMD_PREFIX=
else
CMD_PREFIX=@
endif

all::
	@echo "Compiling..."

all::$(OAL_OBJECTS) $(SOURCES) $(DAEMONNAME) $(OTHER_OBJECTS)

all::
	@echo "Done"

$(DAEMONNAME): $(OAL_OBJECTS) $(OBJECTS_DAEMON) $(OTHER_OBJECTS)
	$(CMD_PREFIX)$(CC) $(DBG_FLAGS) -o $@  $(OAL_OBJECTS) $(OBJECTS_DAEMON) $(OTHER_OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "-Ofast" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

$(OBJDIR)/%.dmn.o: %.cpp $(HEADERS)
	$(CMD_PREFIX)echo $<
	$(CMD_PREFIX)mkdir -p $(OBJDIR)
	$(CMD_PREFIX)$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:
	@export DBG_FLAGS='-g -D _DEBUG'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	$(CMD_PREFIX)rm -f $(DAEMONNAME) $(OBJDIR)/*.o
	$(CMD_PREFIX)rm -rf $(OBJDIR)

