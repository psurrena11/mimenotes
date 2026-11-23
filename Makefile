CXX        := g++
CXXFLAGS   := -std=c++17 -O2 -Wall -Wextra -fPIC $(shell pkg-config --cflags Qt6Widgets)
LDFLAGS    := $(shell pkg-config --libs Qt6Widgets)
TARGET     := notes
PREFIX     ?= $(HOME)/.local
CONFIG_DIR ?= $(HOME)/.config/notes

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -d $(DESTDIR)$(CONFIG_DIR)/themes
	test -f $(DESTDIR)$(CONFIG_DIR)/config.json || install -m 644 config.json $(DESTDIR)$(CONFIG_DIR)/config.json
	install -m 644 themes/*.json $(DESTDIR)$(CONFIG_DIR)/themes/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(TARGET)
