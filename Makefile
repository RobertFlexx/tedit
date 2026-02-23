# ============================================================================
# tedit - Terminal Text Editor
# Makefile
# ============================================================================

# Project info
PROJECT     := tedit
VERSION     := 2.1.0

# Directories
SRC_DIR     := src
INC_DIR     := include
BUILD_DIR   := build
OBJ_DIR     := $(BUILD_DIR)/obj
DEP_DIR     := $(BUILD_DIR)/dep
BIN_DIR     := $(BUILD_DIR)/bin

# Installation directories (can be overridden)
PREFIX      ?= /usr/local
BINDIR      ?= $(PREFIX)/bin
MANDIR      ?= $(PREFIX)/share/man/man1

# Compiler and flags
CXX         := g++
CXXSTD      := -std=c++17
WARNINGS    := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2
INCLUDES    := -I$(INC_DIR)

# Platform detection
UNAME_S     := $(shell uname -s)

# Lua configuration (try pkg-config first, fallback to defaults)
LUA_PKG     := $(shell pkg-config --exists lua5.4 && echo lua5.4 || \
               (pkg-config --exists lua5.3 && echo lua5.3 || \
               (pkg-config --exists lua && echo lua || echo "")))

ifneq ($(LUA_PKG),)
    LUA_CFLAGS  := $(shell pkg-config --cflags $(LUA_PKG))
    LUA_LIBS    := $(shell pkg-config --libs $(LUA_PKG))
else
    # Fallback for systems without pkg-config
    LUA_CFLAGS  :=
    ifeq ($(UNAME_S),Darwin)
        LUA_LIBS := -llua
    else
        LUA_LIBS := -llua -lm -ldl
    endif
endif

# Platform-specific flags
ifeq ($(UNAME_S),Darwin)
    # macOS
    PLATFORM_CFLAGS  :=
    PLATFORM_LDFLAGS :=
else ifeq ($(UNAME_S),Linux)
    # Linux
    PLATFORM_CFLAGS  :=
    PLATFORM_LDFLAGS := -Wl,--as-needed
else
    # Other Unix-like systems (BSD, etc.)
    PLATFORM_CFLAGS  :=
    PLATFORM_LDFLAGS :=
endif

# Build type flags
DEBUG_FLAGS   := -g3 -O0 -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
RELEASE_FLAGS := -O2 -DNDEBUG -march=native
PROFILE_FLAGS := -O2 -g -pg -DNDEBUG

# Default build type
BUILD_TYPE  ?= release

ifeq ($(BUILD_TYPE),debug)
    BUILD_FLAGS := $(DEBUG_FLAGS)
    BUILD_NAME  := debug
else ifeq ($(BUILD_TYPE),profile)
    BUILD_FLAGS := $(PROFILE_FLAGS)
    BUILD_NAME  := profile
else
    BUILD_FLAGS := $(RELEASE_FLAGS)
    BUILD_NAME  := release
endif

# Final flags
CXXFLAGS    := $(CXXSTD) $(WARNINGS) $(BUILD_FLAGS) $(INCLUDES) $(LUA_CFLAGS) $(PLATFORM_CFLAGS)
LDFLAGS     := $(BUILD_FLAGS) $(PLATFORM_LDFLAGS)
LDLIBS      := $(LUA_LIBS)

# Dependency generation flags
DEPFLAGS     = -MT $@ -MMD -MP -MF $(DEP_DIR)/$*.d

# Source files
SOURCES     := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS     := $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPENDS     := $(SOURCES:$(SRC_DIR)/%.cpp=$(DEP_DIR)/%.d)

# Target binary
TARGET      := $(BIN_DIR)/$(PROJECT)

# Colors for pretty output (disable with NOCOLOR=1)
ifndef NOCOLOR
    COLOR_RESET  := \033[0m
    COLOR_GREEN  := \033[32m
    COLOR_YELLOW := \033[33m
    COLOR_CYAN   := \033[36m
    COLOR_BOLD   := \033[1m
else
    COLOR_RESET  :=
    COLOR_GREEN  :=
    COLOR_YELLOW :=
    COLOR_CYAN   :=
    COLOR_BOLD   :=
endif

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean distclean install uninstall debug release profile \
        help info check-deps test rebuild

# Default target
all: $(TARGET)
	@echo "$(COLOR_GREEN)$(COLOR_BOLD)Build complete: $(TARGET) ($(BUILD_NAME))$(COLOR_RESET)"

# Link the target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "$(COLOR_CYAN)Linking$(COLOR_RESET) $(COLOR_BOLD)$@$(COLOR_RESET)"
	@$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) $(DEP_DIR)
	@echo "$(COLOR_YELLOW)Compiling$(COLOR_RESET) $<"
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

# Create directories
$(BIN_DIR) $(OBJ_DIR) $(DEP_DIR):
	@mkdir -p $@

# Include dependency files
-include $(DEPENDS)

# ============================================================================
# Build type shortcuts
# ============================================================================

debug:
	@$(MAKE) BUILD_TYPE=debug

release:
	@$(MAKE) BUILD_TYPE=release

profile:
	@$(MAKE) BUILD_TYPE=profile

rebuild: clean all

# ============================================================================
# Cleaning
# ============================================================================

clean:
	@echo "$(COLOR_YELLOW)Cleaning build files...$(COLOR_RESET)"
	@rm -rf $(BUILD_DIR)

distclean: clean
	@echo "$(COLOR_YELLOW)Removing all generated files...$(COLOR_RESET)"
	@rm -f $(PROJECT) core vgcore.* *.log
	@rm -rf *.dSYM

# ============================================================================
# Installation
# ============================================================================

install: $(TARGET)
	@echo "$(COLOR_CYAN)Installing to $(BINDIR)...$(COLOR_RESET)"
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(PROJECT)
	@echo "$(COLOR_GREEN)Installed $(PROJECT) to $(DESTDIR)$(BINDIR)$(COLOR_RESET)"

uninstall:
	@echo "$(COLOR_YELLOW)Uninstalling from $(BINDIR)...$(COLOR_RESET)"
	@rm -f $(DESTDIR)$(BINDIR)/$(PROJECT)
	@echo "$(COLOR_GREEN)Uninstalled $(PROJECT)$(COLOR_RESET)"

# ============================================================================
# Development helpers
# ============================================================================

# Check dependencies are available
check-deps:
	@echo "$(COLOR_CYAN)Checking dependencies...$(COLOR_RESET)"
	@command -v $(CXX) >/dev/null 2>&1 || \
		{ echo "$(COLOR_BOLD)Error:$(COLOR_RESET) $(CXX) not found"; exit 1; }
	@echo "  $(COLOR_GREEN)✓$(COLOR_RESET) Compiler: $(CXX) $(shell $(CXX) --version | head -n1)"
ifeq ($(LUA_PKG),)
	@echo "  $(COLOR_YELLOW)!$(COLOR_RESET) Lua: using fallback flags (pkg-config not found)"
else
	@echo "  $(COLOR_GREEN)✓$(COLOR_RESET) Lua: $(LUA_PKG) (via pkg-config)"
endif
	@echo "  $(COLOR_GREEN)✓$(COLOR_RESET) Platform: $(UNAME_S)"
	@echo "$(COLOR_GREEN)All dependencies satisfied$(COLOR_RESET)"

# Run a quick sanity test
test: $(TARGET)
	@echo "$(COLOR_CYAN)Running basic tests...$(COLOR_RESET)"
	@echo "version" | $(TARGET) /dev/null 2>&1 | grep -q "tedit" && \
		echo "  $(COLOR_GREEN)✓$(COLOR_RESET) Version check passed" || \
		{ echo "  $(COLOR_BOLD)✗$(COLOR_RESET) Version check failed"; exit 1; }
	@echo "quit" | $(TARGET) /dev/null 2>&1 && \
		echo "  $(COLOR_GREEN)✓$(COLOR_RESET) Quit test passed" || \
		{ echo "  $(COLOR_BOLD)✗$(COLOR_RESET) Quit test failed"; exit 1; }
	@echo "$(COLOR_GREEN)All tests passed$(COLOR_RESET)"

# Show build configuration
info:
	@echo "$(COLOR_BOLD)tedit $(VERSION) - Build Configuration$(COLOR_RESET)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "$(COLOR_CYAN)Directories:$(COLOR_RESET)"
	@echo "  Source:      $(SRC_DIR)/"
	@echo "  Include:     $(INC_DIR)/"
	@echo "  Build:       $(BUILD_DIR)/"
	@echo "  Install:     $(BINDIR)/"
	@echo ""
	@echo "$(COLOR_CYAN)Compiler:$(COLOR_RESET)"
	@echo "  CXX:         $(CXX)"
	@echo "  CXXSTD:      $(CXXSTD)"
	@echo "  BUILD_TYPE:  $(BUILD_TYPE)"
	@echo ""
	@echo "$(COLOR_CYAN)Flags:$(COLOR_RESET)"
	@echo "  CXXFLAGS:    $(CXXFLAGS)"
	@echo "  LDFLAGS:     $(LDFLAGS)"
	@echo "  LDLIBS:      $(LDLIBS)"
	@echo ""
	@echo "$(COLOR_CYAN)Sources:$(COLOR_RESET)"
	@for src in $(SOURCES); do echo "  $$src"; done
	@echo ""
	@echo "$(COLOR_CYAN)Headers:$(COLOR_RESET)"
	@for hdr in $(wildcard $(INC_DIR)/*.hpp); do echo "  $$hdr"; done

# ============================================================================
# Help
# ============================================================================

help:
	@echo "$(COLOR_BOLD)tedit $(VERSION) - Build System$(COLOR_RESET)"
	@echo ""
	@echo "$(COLOR_CYAN)Usage:$(COLOR_RESET)"
	@echo "  make [target] [options]"
	@echo ""
	@echo "$(COLOR_CYAN)Targets:$(COLOR_RESET)"
	@echo "  all          Build the project (default)"
	@echo "  debug        Build with debug flags"
	@echo "  release      Build with release optimizations"
	@echo "  profile      Build with profiling enabled"
	@echo "  rebuild      Clean and rebuild"
	@echo "  clean        Remove build files"
	@echo "  distclean    Remove all generated files"
	@echo "  install      Install to system (may need sudo)"
	@echo "  uninstall    Remove from system"
	@echo "  check-deps   Verify build dependencies"
	@echo "  test         Run basic tests"
	@echo "  info         Show build configuration"
	@echo "  help         Show this help"
	@echo ""
	@echo "$(COLOR_CYAN)Options:$(COLOR_RESET)"
	@echo "  BUILD_TYPE=  debug|release|profile (default: release)"
	@echo "  PREFIX=      Installation prefix (default: /usr/local)"
	@echo "  CXX=         C++ compiler (default: g++)"
	@echo "  NOCOLOR=1    Disable colored output"
	@echo ""
	@echo "$(COLOR_CYAN)Examples:$(COLOR_RESET)"
	@echo "  make                    # Build release"
	@echo "  make debug              # Build with debug symbols"
	@echo "  make -j\$$(nproc)         # Parallel build"
	@echo "  make install PREFIX=~   # Install to home directory"
	@echo "  make CXX=clang++        # Use Clang compiler"

# ============================================================================
# Special targets
# ============================================================================

# Prevent make from deleting intermediate files
.SECONDARY: $(OBJECTS)

# Don't use built-in rules
.SUFFIXES:

# Targets that don't produce files with their name
.PHONY: all clean distclean install uninstall debug release profile \
        help info check-deps test rebuild
