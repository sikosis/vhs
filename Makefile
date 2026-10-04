CXX ?= c++
VERSION := $(strip $(shell cat VERSION))
CPPFLAGS ?= -Iinclude -D_XOPEN_SOURCE=600 -DVHS_VERSION=\"$(VERSION)\"
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS ?=

SOURCES := src/main.cpp src/tape.cpp src/executor.cpp src/terminal.cpp src/renderer.cpp \
	src/video.cpp

SOURCES += src/record.cpp

ifeq ($(shell uname -s),Haiku)
LDLIBS += -lbe -ltranslation
endif
OBJECTS := $(SOURCES:.cpp=.o)
TARGET := vhs
TEST_TARGETS := tests/tape_test tests/terminal_screen_test

PREFIX ?= /boot/system/non-packaged
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
MANDIR ?= $(DATADIR)/man
DOCDIR ?= $(DATADIR)/doc/vhs

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

src/main.o: VERSION

tests/tape_test: tests/tape_test.cpp src/tape.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tests/terminal_screen_test: tests/terminal_screen_test.cpp src/terminal.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: $(TARGET) $(TEST_TARGETS)
	./tests/tape_test
	./tests/terminal_screen_test
	sh ./tests/test_cli.sh ./$(TARGET)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1 $(DESTDIR)$(DOCDIR)
	mkdir -p $(DESTDIR)$(DATADIR)/bash-completion/completions
	mkdir -p $(DESTDIR)$(DATADIR)/zsh/site-functions
	mkdir -p $(DESTDIR)$(DATADIR)/fish/vendor_completions.d
	cp $(TARGET) $(DESTDIR)$(BINDIR)/vhs
	cp docs/vhs.1 $(DESTDIR)$(MANDIR)/man1/vhs.1
	cp completions/vhs.bash $(DESTDIR)$(DATADIR)/bash-completion/completions/vhs
	cp completions/_vhs $(DESTDIR)$(DATADIR)/zsh/site-functions/_vhs
	cp completions/vhs.fish $(DESTDIR)$(DATADIR)/fish/vendor_completions.d/vhs.fish
	cp README.md docs/ROADMAP.md docs/RELEASE.md CHANGELOG.md $(DESTDIR)$(DOCDIR)/
	cp -R examples $(DESTDIR)$(DOCDIR)/examples

clean:
	rm -f $(TARGET) $(OBJECTS) $(TEST_TARGETS)
