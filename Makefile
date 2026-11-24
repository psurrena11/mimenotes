CXX         := g++
CXXFLAGS    := -std=c++17 -O2 -Wall -Wextra -fPIC $(shell pkg-config --cflags Qt6Widgets Qt6Svg)
LDFLAGS     := $(shell pkg-config --libs Qt6Widgets Qt6Svg)
QT6_LIBEXEC := $(shell pkg-config --variable=libexecdir Qt6Core 2>/dev/null)
RCC         ?= $(firstword $(wildcard $(QT6_LIBEXEC)/rcc /usr/lib/qt6/rcc /usr/lib/qt6/libexec/rcc /usr/lib64/qt6/libexec/rcc) $(shell command -v rcc-qt6 2>/dev/null || command -v rcc 2>/dev/null))
TARGET      := mimenotes
QRC_DEPS    := resources.qrc mimenotes.svg config.json $(wildcard assets/*.svg themes/*.json)
PREFIX      ?= $(HOME)/.local
CONFIG_DIR  ?= $(HOME)/.config/mimenotes
APPS_DIR    ?= $(PREFIX)/share/applications
ICONS_DIR   ?= $(PREFIX)/share/icons/hicolor/scalable/apps

.PHONY: all clean install uninstall

all: $(TARGET)

qrc_resources.cpp: $(QRC_DEPS)
	$(RCC) --name resources -o qrc_resources.cpp resources.qrc

$(TARGET): main.cpp qrc_resources.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp qrc_resources.cpp $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -d $(DESTDIR)$(CONFIG_DIR)/themes
	install -d $(DESTDIR)$(CONFIG_DIR)/assets
	install -m 644 config.json $(DESTDIR)$(CONFIG_DIR)/config.json
	install -m 644 themes/*.json $(DESTDIR)$(CONFIG_DIR)/themes/
	install -m 644 assets/*.svg $(DESTDIR)$(CONFIG_DIR)/assets/
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
	rm -f $(TARGET) notes qrc_resources.cpp
