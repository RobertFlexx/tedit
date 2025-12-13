# Makefile — tedit (C++17, interactive CLI editor)
# Portable + pkg-config aware Lua detection.
# Usage:
#   make                # builds
#   make release        # optimized build
#   make debug          # debug build
#   make PREFIX=/usr    # install prefix
#   make DESTDIR=/tmp/pkg install
#   make LUA_CFLAGS=... LUA_LIBS=...   # manual Lua override

SHELL     := /bin/sh

APP       := tedit
TARGET    := $(APP)
SRC       := tedit.cpp
OBJ       := $(SRC:.cpp=.o)

PREFIX    ?= /usr/local
DESTDIR   ?=

# Toolchain
CXX       ?= c++
AR        ?= ar
RM        ?= rm -f
MKDIR_P   ?= mkdir -p
INSTALL   ?= install
STRIP     ?= strip

# Build modes
MODE      ?= release

# Common flags (allow user override/extend)
CPPFLAGS  ?=
CXXFLAGS  ?= -std=c++17 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS   ?=
LDLIBS    ?=

# Mode-specific
ifeq ($(MODE),debug)
  CXXFLAGS += -O0 -g3
else ifeq ($(MODE),release)
  CXXFLAGS += -O2 -g
else
  # custom MODE allowed
  CXXFLAGS += -O2 -g
endif

# Optional hardening (safe defaults; can disable: make HARDEN=0)
HARDEN ?= 1
ifeq ($(HARDEN),1)
  CXXFLAGS += -fno-omit-frame-pointer
endif

# ----------------------------
# Lua detection
# Prefer pkg-config if available. Tries lua5.4, lua5.3, lua, luajit.
# Users may always override LUA_CFLAGS/LUA_LIBS explicitly.
# ----------------------------
PKG_CONFIG ?= pkg-config

LUA_PKG   ?=
LUA_CFLAGS ?=
LUA_LIBS   ?=

# only auto-detect if user didn't set LUA_* vars
ifeq ($(strip $(LUA_CFLAGS)$(LUA_LIBS)),)
  ifneq ($(shell command -v $(PKG_CONFIG) >/dev/null 2>&1 && echo yes),)
    LUA_PKG := $(shell \
      $(PKG_CONFIG) --exists lua5.4 2>/dev/null && echo lua5.4 || \
      $(PKG_CONFIG) --exists lua5.3 2>/dev/null && echo lua5.3 || \
      $(PKG_CONFIG) --exists lua     2>/dev/null && echo lua     || \
      $(PKG_CONFIG) --exists luajit  2>/dev/null && echo luajit  || \
      echo "")
    ifneq ($(strip $(LUA_PKG)),)
      LUA_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(LUA_PKG) 2>/dev/null)
      LUA_LIBS   := $(shell $(PKG_CONFIG) --libs   $(LUA_PKG) 2>/dev/null)
    endif
  endif
endif

# final fallback (keeps your original behavior but only if needed)
ifeq ($(strip $(LUA_CFLAGS)$(LUA_LIBS)),)
  LUA_CFLAGS := -I/usr/include/lua5.4
  LUA_LIBS   := -llua5.4 -ldl -lm
endif

CPPFLAGS += $(LUA_CFLAGS)
LDLIBS   += $(LUA_LIBS)

# ----------------------------
# Defaults
# ----------------------------
MANPAGE   ?= tedit.1
BINDIR    := $(DESTDIR)$(PREFIX)/bin
MANDIR    := $(DESTDIR)$(PREFIX)/share/man/man1

# ----------------------------
# Targets
# ----------------------------
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

release:
	$(MAKE) MODE=release all

debug:
	$(MAKE) MODE=debug all

print-vars:
	@echo "CXX=$(CXX)"
	@echo "MODE=$(MODE)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "LDLIBS=$(LDLIBS)"
	@echo "LUA_PKG=$(LUA_PKG)"
	@echo "PREFIX=$(PREFIX)"
	@echo "DESTDIR=$(DESTDIR)"

run: $(TARGET)
	./$(TARGET) notes.txt

install: $(TARGET)
	@$(MKDIR_P) "$(BINDIR)"
	@$(INSTALL) -m 0755 "$(TARGET)" "$(BINDIR)/$(TARGET)"
	@# man page is optional
	@if [ -f "$(MANPAGE)" ]; then \
	  $(MKDIR_P) "$(MANDIR)"; \
	  $(INSTALL) -m 0644 "$(MANPAGE)" "$(MANDIR)/$(MANPAGE)"; \
	else \
	  echo "note: $(MANPAGE) not found; skipping man install"; \
	fi

uninstall:
	@$(RM) "$(BINDIR)/$(TARGET)"
	@$(RM) "$(MANDIR)/$(MANPAGE)"

strip: $(TARGET)
	@command -v "$(STRIP)" >/dev/null 2>&1 && "$(STRIP)" "$(TARGET)" 2>/dev/null || true

format:
	@command -v clang-format >/dev/null 2>&1 && clang-format -i $(SRC) || echo "clang-format not found; skipping"

clean:
	@$(RM) $(OBJ) $(TARGET)

.PHONY: all release debug run install uninstall clean format strip print-vars
