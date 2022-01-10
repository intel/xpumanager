EXE = pcm-iio-gpu.x

UNAME:=$(shell uname)

COMMON_FLAGS = -O3
CFLAGS += $(COMMON_FLAGS)
CXXFLAGS += $(COMMON_FLAGS) -std=c++11

ifeq ($(UNAME), Linux)
CFLAGS += -pthread
LIB= -lpthread
CXXFLAGS += -pthread
endif

COMMON_OBJS = msr.o cpucounters.o pci.o mmio.o bw.o utils.o topology.o debug.o threadpool.o resctrl.o

all: $(EXE)
%.x: %.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIB)

clean:
	@rm -rf *.x *.o 
