# Makefile — tedit (C++17, interactive CLI editor)

CXX      ?= c++
CXXFLAGS ?= -O2 -g -Wall -Wextra -Wpedantic -std=c++17 \
            -D_POSIX_C_SOURCE=200809L
LDFLAGS  ?=
LDLIBS   ?=

TARGET   := tedit
SRC      := tedit.cpp
OBJ      := $(SRC:.cpp=.o)
PREFIX   ?= /usr/local

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) notes.txt

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 0644 tedit.1 $(DESTDIR)$(PREFIX)/share/man/man1/tedit.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/tedit.1

format:
	@command -v clang-format >/dev/null 2>&1 && clang-format -i $(SRC) || echo "clang-format not found; skipping"

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all run install uninstall clean format
