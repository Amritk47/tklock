# tklock Makefile
# GTK3 screen locker utility

VERSION = 1.0.0

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -DVERSION=\"$(VERSION)\"
LDFLAGS =

# Package dependencies
PKG_DEPS = gtk+-3.0 xcb
PKG_CFLAGS = $(shell pkg-config --cflags $(PKG_DEPS))
PKG_LIBS = $(shell pkg-config --libs $(PKG_DEPS))

# Check for explicit_bzero
HAVE_EXPLICIT_BZERO := $(shell echo 'int main(){explicit_bzero(0,0);return 0;}' | $(CC) -x c - -o /dev/null 2>/dev/null && echo 1)
ifeq ($(HAVE_EXPLICIT_BZERO),1)
CFLAGS += -DHAVE_EXPLICIT_BZERO
endif

# Source files
SRCDIR = src
BUILDDIR = build

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/config.c \
          $(SRCDIR)/security.c \
          $(SRCDIR)/ui.c \
          $(SRCDIR)/auth.c

OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

TARGET = $(BUILDDIR)/tklock

.PHONY: all clean install check-deps

all: check-deps $(TARGET)

check-deps:
	@pkg-config --exists $(PKG_DEPS) || (echo "Missing dependencies. Install with:"; \
		echo "  sudo apt install libgtk-3-dev libxcb1-dev"; \
		exit 1)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile source files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

# Link
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(PKG_LIBS) -o $@
	@echo "Build complete: $(TARGET)"

clean:
	rm -rf $(BUILDDIR)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/tklock
