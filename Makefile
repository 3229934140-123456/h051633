# Moonjit Makefile
# Usage: make [all|moonjit|host_demo|clean]

CC ?= tcc
CFLAGS ?= -Isrc -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=

# Source files
LIB_SOURCES = \
    src/memory.c \
    src/value.c \
    src/object.c \
    src/chunk.c \
    src/table.c \
    src/lexer.c \
    src/compiler.c \
    src/vm.c \
    src/gc.c \
    src/host.c

MAIN_SOURCES = src/main.c $(LIB_SOURCES)
DEMO_SOURCES = examples/host_demo.c $(LIB_SOURCES)

# Targets
.PHONY: all clean

all: moonjit host_demo

moonjit: $(MAIN_SOURCES)
	$(CC) $(CFLAGS) -o moonjit.exe $(MAIN_SOURCES) $(LDFLAGS)
	@echo "Built moonjit.exe"

host_demo: $(DEMO_SOURCES)
	$(CC) $(CFLAGS) -o host_demo.exe $(DEMO_SOURCES) $(LDFLAGS)
	@echo "Built host_demo.exe"

clean:
	-@rm -f moonjit.exe host_demo.exe
	@echo "Cleaned build artifacts"
