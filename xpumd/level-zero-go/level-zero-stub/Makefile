# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Intel Corporation

CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -fPIC -D_POSIX_C_SOURCE=200809L -I.. $(shell pkg-config --cflags libcyaml)
LDFLAGS = -shared
LDLIBS  = -lpthread $(shell pkg-config --libs libcyaml)

TEST_CFLAGS  = -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -I.. $(shell pkg-config --cflags libcyaml)
TEST_LDFLAGS = -Wl,-rpath,.
TEST_LDLIBS  = -L. -lze_stub $(shell pkg-config --libs libcyaml)

LIB    = libze_stub.so
SOLINK = libze_loader.so.1
SRCS  = zes_stub.c sysman_state.c
OBJS  = $(SRCS:.c=.o)
TESTS = test_sysman

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[^#:. ]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

.PHONY: all
all: $(LIB)

$(LIB): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	ln -sf $(LIB) $(SOLINK)

%.o: %.c sysman_state.h sysman_state_cyaml.h zes_stub.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(TESTS): %: %.c $(LIB) zes_stub.h
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_LDFLAGS) $(TEST_LDLIBS)

.PHONY: test
test: $(TESTS)
	@for t in $(TESTS); do echo "--- $$t ---"; ./$$t || exit 1; done

.PHONY: valgrind
valgrind: $(TESTS)
	@for t in $(TESTS); do \
		echo "--- memcheck: $$t ---"; \
		valgrind --tool=memcheck --leak-check=full --track-origins=yes --error-exitcode=1 ./$$t || exit 1; \
		echo "--- helgrind: $$t ---"; \
		valgrind --tool=helgrind --error-exitcode=1 ./$$t || exit 1; \
		echo "--- drd: $$t ---"; \
		valgrind --tool=drd --error-exitcode=1 ./$$t || exit 1; \
	done

.PHONY: generate
generate:
	python3 ../hack/generate-cyaml.py \
		--header-file ../level-zero/ze_api.h \
		--header-file ../level-zero/zes_api.h \
		--header-file sysman_state.h \
		--root-struct sysman_state_t \
		--output-file sysman_state_cyaml.h
	clang-format -i sysman_state_cyaml.h

.PHONY: format
format:
	@clang-format -i *.c *.h

.PHONY: clean
clean:
	rm -f $(OBJS) $(LIB) $(SOLINK) $(TESTS)
