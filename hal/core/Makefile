# Copyright (C) 2024 Intel Corporation
# SPDX-License-Identifier: MIT

# Directories
SRCDIR := .
OBJDIR := obj
OALDIR := ../../oal/lin
FWDIR  := ../fwupd

# Compiler and Flags
CXX := g++
export DBG_FLAGS?=-Ofast
CXXFLAGS := -Wall -fPIC -I. -I$(OALDIR) -I$(FWDIR) -I$(LEVELZERO)/include -I$(IGSC)/include
LIBS :=-L$(LEVELZERO)/build/lib -lze_loader -L$(IGSC)/build/lib -ligsc
INCLUDEDIR := include

# Target libraries
TARGET_SHARED := libxpum.so
TARGET_STATIC := libxpum.a

# Find source files, create a list of object files
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
HEADERS := $(wildcard $(SRCDIR)/*.h $(OALDIR)/*.h)
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(OBJECTS:.o=.d)  # Dependency files corresponding to object files
OAL_OBJECTS := $(wildcard $(OALDIR)/$(OBJDIR)/*.o)
FWUPD_OBJECTS := $(wildcard $(FWDIR)/$(OBJDIR)/*.o)

# Check if VERBOSE is set to 1, if not, silence the commands
ifeq ($(VERBOSE),1)
CMD_PREFIX=
else
CMD_PREFIX=@
endif

# Default target
all: $(TARGET_SHARED) $(TARGET_STATIC)

# Shared library
$(TARGET_SHARED): $(OBJECTS) $(OAL_OBJECTS) $(FWUPD_OBJECTS)
	$(CMD_PREFIX)echo "Creating shared library..."
	$(CMD_PREFIX)$(CXX) -Wall -shared $(DBG_FLAGS) -o $@ $^ $(LIBS)
	@if [ "$(DBG_FLAGS)" = "-Ofast" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

# Static library
$(TARGET_STATIC): $(OBJECTS) $(OAL_OBJECTS) $(FWUPD_OBJECTS)
	$(CMD_PREFIX)echo "Creating static library..."
	$(CMD_PREFIX)ar rcs $@ $^
	@if [ "$(DBG_FLAGS)" = "-Ofast" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

# Compile source files into object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	$(CMD_PREFIX)echo "Compiling $<"
	$(CMD_PREFIX)mkdir -p $(OBJDIR)
	$(CMD_PREFIX)$(CXX) $(CXXFLAGS) $(DBG_FLAGS) -c $< -o $@

# Include the .d files
-include $(DEPS)

debug:
	@export DBG_FLAGS='-g -D DEBUGON'; \
	$(MAKE)

# Clean up build artifacts
clean:
	$(CMD_PREFIX)echo "Cleaning..."
	$(CMD_PREFIX)rm -f $(TARGET_SHARED) $(TARGET_STATIC) $(OBJECTS) $(DEPS)
	$(CMD_PREFIX)rm -rf $(OBJDIR)


# Phony targets
.PHONY: all clean
