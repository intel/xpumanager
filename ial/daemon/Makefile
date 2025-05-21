# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

# Directories
SRCDIR := .
OBJDIR := obj
CMNDIR := ../cmn
OALDIR := ../../oal/lin
HALCORE := ../../hal/core
FWDIR  := ../../hal/fwupd

export PWD=$(shell pwd)
DAEMONNAME=xpumd
MAKE += --no-print-directory
export CC=g++
export DBG_FLAGS?=-Ofast
export LIBS=-L$(HALCORE) -lxpum -L$(LEVELZERO)/build/lib -lze_loader -L$(IGSC)/build/lib -ligsc
export INCLUDES=-I. -I$(CMNDIR) -I$(OALDIR) -I$(HALCORE) -I$(FWDIR) -I$(LEVELZERO)/include -I$(IGSC)/include
export CFLAGS=-c -Wall
export SOURCES := $(wildcard $(SRCDIR)/*.cpp)
export HEADERS := $(wildcard $(SRCDIR)/*.h)
export DEFINES_DMN = -DDAEMONMODE
OBJECTS_DAEMON := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.dmn.o)
OTHER_OBJECTS = $(wildcard $(CMNDIR)/$(OBJDIR)/*.o)
OAL_OBJECTS = $(wildcard $(OALDIR)/$(OBJDIR)/*.o)

all::
	@echo "Compiling..."

all::$(OAL_OBJECTS) $(SOURCES) $(DAEMONNAME) $(OTHER_OBJECTS)

all::
	@echo "Done"

$(DAEMONNAME):  $(OAL_OBJECTS) $(OBJECTS_DAEMON) $(OTHER_OBJECTS)
	@$(CC) $(DBG_FLAGS) -o $@  $(OAL_OBJECTS) $(OBJECTS_DAEMON) $(OTHER_OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "-Ofast" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

$(OBJDIR)/%.dmn.o: %.cpp $(HEADERS)
	@echo $<
	@mkdir -p $(OBJDIR)
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

clean:
	@echo "Cleaning..."
	@rm -f $(DAEMONNAME) $(OBJDIR)/*.o
	@rm -rf $(OBJDIR)

