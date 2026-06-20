SHELL     := /bin/sh

APP       := tedit
TARGET    := $(APP)
SRC       := src/tedit.cpp
OBJ       := $(SRC:.cpp=.o)
SRC_ALL   := $(wildcard src/*.cpp)

PREFIX    ?= /usr/local
DESTDIR   ?=

CXX       ?= c++
AR        ?= ar
RM        ?= rm -f
MKDIR_P   ?= mkdir -p
INSTALL   ?= install
STRIP     ?= strip

MODE      ?= release

CPPFLAGS  ?=
CXXFLAGS  ?= -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS   ?=
LDLIBS    ?=

ifeq ($(MODE),debug)
  CXXFLAGS += -O0 -g3
else ifeq ($(MODE),release)
  CXXFLAGS += -O2 -g
else
  CXXFLAGS += -O2 -g
endif

HARDEN ?= 1
ifeq ($(HARDEN),1)
  CXXFLAGS += -fno-omit-frame-pointer
endif

CXXFLAGS += -fPIE
LDFLAGS  += -pie

PKG_CONFIG ?= pkg-config

LUA_PKG   ?=
LUA_CFLAGS ?=
LUA_LIBS   ?=

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

ifeq ($(strip $(LUA_CFLAGS)$(LUA_LIBS)),)
  LUA_CFLAGS := -I/usr/include/lua5.4
  LUA_LIBS   := -llua5.4 -ldl -lm
endif

CPPFLAGS += $(LUA_CFLAGS)
LDLIBS   += $(LUA_LIBS)

MANPAGE   ?= mandoc/tedit.1
MANPAGE_FILE := $(notdir $(MANPAGE))
BINDIR    := $(DESTDIR)$(PREFIX)/bin
MANDIR    := $(DESTDIR)$(PREFIX)/share/man/man1

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
	@if [ -f "$(MANPAGE)" ]; then \
	  $(MKDIR_P) "$(MANDIR)"; \
	  $(INSTALL) -m 0644 "$(MANPAGE)" "$(MANDIR)/$(MANPAGE_FILE)"; \
	else \
	  echo "note: $(MANPAGE) not found; skipping man install"; \
	fi

uninstall:
	@$(RM) "$(BINDIR)/$(TARGET)"
	@$(RM) "$(MANDIR)/$(MANPAGE_FILE)"

strip: $(TARGET)
	@command -v "$(STRIP)" >/dev/null 2>&1 && "$(STRIP)" "$(TARGET)" 2>/dev/null || true

format:
	@command -v clang-format >/dev/null 2>&1 && clang-format -i $(SRC_ALL) || echo "clang-format not found; skipping"

clean:
	@$(RM) $(OBJ) $(TARGET)

.PHONY: all release debug run install uninstall clean format strip print-vars
