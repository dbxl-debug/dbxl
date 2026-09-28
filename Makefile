# dbxl debugger - plain Makefile

CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc -I$(BUILD)/gen $(shell pkg-config --cflags x11 xrender)
LDLIBS  += $(shell pkg-config --libs x11 xrender) -lm

BUILD   := build
PROG    := dbxl

SRCS := \
	src/main.c \
	src/opts.c \
	src/ui.c \
	src/debugger.c \
	src/data.c \
	src/backend/signals.c \
	src/backend/gdbmi/gdbmi.c \
	src/backend/gdbmi/mi_parse.c \
	src/core/commandlist.c \
	src/core/layout.c \
	src/core/options.c \
	src/core/resources.c \
	src/core/source.c \
	src/core/value.c \
	src/xtk/chip.c \
	src/xtk/dialog.c \
	src/xtk/display.c \
	src/xtk/frame.c \
	src/xtk/gesture.c \
	src/xtk/glyphs.c \
	src/xtk/lineedit.c \
	src/xtk/loop.c \
	src/xtk/menu.c \
	src/xtk/pane.c \
	src/xtk/scrollbar.c \
	src/xtk/surface.c

OBJS := $(SRCS:%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

# Programs the tests debug.  Built in their own directory so their source
# file is "./test.c" from there, as in the xldb recordings.
TESTPROGS := tests/progs/test tests/progs/rich

.PHONY: all clean visual test progs

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

# dbxl.py goes into the binary as an array of lines (C strings are
# limited to 4095 characters).
$(BUILD)/gen/dbxl_py.h: src/backend/gdbmi/dbxl.py Makefile
	@mkdir -p $(dir $@)
	{ echo 'static const char *const dbxl_py[] = {'; \
	  sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/    "/' -e 's/$$/\\n",/' $<; \
	  echo '    NULL'; echo '};'; } > $@

$(BUILD)/src/backend/gdbmi/gdbmi.o: $(BUILD)/gen/dbxl_py.h

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

progs: $(TESTPROGS)

tests/progs/%: tests/progs/%.c
	cd tests/progs && $(CC) -g -O0 -o $* $*.c

$(BUILD)/mi_parse_test: tests/unit/mi_parse_test.c src/backend/gdbmi/mi_parse.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

$(BUILD)/format_test: tests/unit/format_test.c src/core/value.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ -lm

test: $(BUILD)/mi_parse_test $(BUILD)/format_test
	$(BUILD)/mi_parse_test
	$(BUILD)/format_test

visual: $(PROG) progs
	tests/visual/run.sh

clean:
	rm -rf $(BUILD) $(PROG) $(TESTPROGS)

-include $(DEPS)
