UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    CC=gcc
else ifeq ($(UNAME_S),Darwin)
    CC=clang
endif

# printf.S is hand-written x86_64 assembly. Build/include it only on x86_64
# hosts; on other architectures the pure-C fallback in runtime.c is used.
ifeq ($(UNAME_M),x86_64)
    PRINTF_OBJ = printf.o
endif

DISABLE_WARNINGS=-Wno-shift-negative-value
COMMON_FLAGS=$(DISABLE_WARNINGS) -g -fstack-protector-all --std=c11
PROD_FLAGS=$(COMMON_FLAGS) -DLAMA_ENV
TEST_FLAGS=$(COMMON_FLAGS) -DDEBUG_VERSION
UNIT_TESTS_FLAGS=$(TEST_FLAGS)
INVARIANTS_CHECK_FLAGS=$(TEST_FLAGS) -DFULL_INVARIANT_CHECKS

all: gc.o runtime.o $(PRINTF_OBJ)
	ar rc runtime.a runtime.o gc.o $(PRINTF_OBJ)

gc.o: gc.c gc.h
	$(CC) $(PROD_FLAGS) -c gc.c -o gc.o

runtime.o: runtime.c runtime.h
	$(CC) $(PROD_FLAGS) -c runtime.c -o runtime.o

printf.o: printf.S
	$(CC) $(PROD_FLAGS) -Wa,--noexecstack -x assembler-with-cpp -c -g printf.S -o printf.o

clean:
	$(RM) *.a *.o *~ negative_scenarios/*.err

