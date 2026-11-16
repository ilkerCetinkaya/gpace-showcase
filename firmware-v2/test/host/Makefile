CC ?= cc
ROOT := ../..
INCLUDES := -I$(ROOT)/components/gpace_ubx/include -I$(ROOT)/components/gpace_bmi270/include -I$(ROOT)/components/gpace_frame/include -I$(ROOT)/components/gpace_battery/include
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined $(INCLUDES)
SOURCES := test_main.c test_ubx.c test_session.c test_bmi270.c test_frame.c test_battery.c fake_gnss.c \
	$(ROOT)/components/gpace_ubx/gpace_ubx.c $(ROOT)/components/gpace_ubx/gpace_ubx_session.c \
	$(ROOT)/components/gpace_bmi270/gpace_bmi270.c $(ROOT)/components/gpace_frame/gpace_frame.c $(ROOT)/components/gpace_battery/gpace_battery.c

all: run

build/gpace_tests: $(SOURCES) $(wildcard *.h)
	mkdir -p build
	$(CC) $(CFLAGS) $(SOURCES) -o $@

run: build/gpace_tests
	./build/gpace_tests

clean:
	rm -rf build

.PHONY: all run clean
