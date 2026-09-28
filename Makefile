# dbxl debugger - plain Makefile

CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc $(shell pkg-config --cflags x11 xrender)
LDLIBS  += $(shell pkg-config --libs x11 xrender)

BUILD   := build
PROG    := dbxl

SRCS := \
	src/main.c \
	src/opts.c \
	src/core/layout.c \
	src/panes/commands.c \
	src/xtk/display.c \
	src/xtk/frame.c \
	src/xtk/pane.c \
	src/xtk/chip.c \
	src/xtk/scrollbar.c \
	src/xtk/surface.c \
	src/xtk/glyphs.c \
	src/xtk/loop.c

OBJS := $(SRCS:%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean visual

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

visual: $(PROG)
	tests/visual/run.sh

clean:
	rm -rf $(BUILD) $(PROG)

-include $(DEPS)
