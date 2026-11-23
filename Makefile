CXX        := g++
CXXFLAGS   := -std=c++17 -O2 -Wall -Wextra -fPIC $(shell pkg-config --cflags Qt6Widgets)
LDFLAGS    := $(shell pkg-config --libs Qt6Widgets)
TARGET     := mimenotes
PREFIX     ?= $(HOME)/.local
CONFIG_DIR ?= $(HOME)/.config/mimenotes
APPS_DIR   ?= $(PREFIX)/share/applications
ICONS_DIR  ?= $(PREFIX)/share/icons/hicolor/scalable/apps

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
	install -d $(DESTDIR)$(ICONS_DIR)
	install -m 644 mimenotes.svg $(DESTDIR)$(ICONS_DIR)/mimenotes.svg
	install -d $(DESTDIR)$(APPS_DIR)
	sed "s|^Exec=mimenotes|Exec=$(PREFIX)/bin/mimenotes|" mimenotes.desktop > $(DESTDIR)$(APPS_DIR)/mimenotes.desktop
	chmod 644 $(DESTDIR)$(APPS_DIR)/mimenotes.desktop
	-update-desktop-database $(DESTDIR)$(APPS_DIR) 2>/dev/null || true

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)$(APPS_DIR)/mimenotes.desktop
	rm -f $(DESTDIR)$(ICONS_DIR)/mimenotes.svg
	-update-desktop-database $(DESTDIR)$(APPS_DIR) 2>/dev/null || true

clean:
	rm -f $(TARGET) notes
