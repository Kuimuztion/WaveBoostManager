CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g
CFLAGS += -Iinclude
PKG_CFLAGS = $(shell pkg-config --cflags gtk4 libcjson)
PKG_LIBS = $(shell pkg-config --libs gtk4 libcjson)

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = builds
OBJ_DIR = $(BUILD_DIR)/obj

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

TARGET = $(BUILD_DIR)/wave_boost_manager
TEST_TARGET = $(BUILD_DIR)/test_core

PREFIX ?= $(HOME)/.local
BINDIR = $(PREFIX)/bin
APPDIR = $(PREFIX)/share/applications
ICONDIR = $(PREFIX)/share/icons/hicolor/128x128/apps
DESKTOP_FILE = $(APPDIR)/waveboostmanager.desktop

.PHONY: all clean run test install uninstall

all: $(TARGET)

$(TARGET): $(OBJ) waveboostmanager.png
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJ) -o $@ $(PKG_LIBS)
	@cp -f waveboostmanager.png $(BUILD_DIR)/waveboostmanager.png
	@echo "Build successful! Output placed in: $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	@echo "Running tests in $(BUILD_DIR)..."
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_DIR)/test_core.c $(SRC_DIR)/models.c $(SRC_DIR)/data_service.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) $^ -o $@ $(PKG_LIBS)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	mkdir -p $(BINDIR)
	mkdir -p $(APPDIR)
	mkdir -p $(ICONDIR)
	install -m 755 $(TARGET) $(BINDIR)/waveboostmanager
	install -m 644 waveboostmanager.png $(ICONDIR)/waveboostmanager.png
	@echo "Generating desktop entry with absolute executable path..."
	@echo "[Desktop Entry]" > $(DESKTOP_FILE)
	@echo "Name=WaveBoostManager" >> $(DESKTOP_FILE)
	@echo "Name[zh_CN]=代肝任务面板" >> $(DESKTOP_FILE)
	@echo "Comment=Game boost and commission task management tool" >> $(DESKTOP_FILE)
	@echo "Comment[zh_CN]=代肝任务面板管理工具" >> $(DESKTOP_FILE)
	@echo "Exec=$(BINDIR)/waveboostmanager" >> $(DESKTOP_FILE)
	@echo "Icon=$(ICONDIR)/waveboostmanager.png" >> $(DESKTOP_FILE)
	@echo "Terminal=false" >> $(DESKTOP_FILE)
	@echo "Type=Application" >> $(DESKTOP_FILE)
	@echo "Categories=Game;Utility;" >> $(DESKTOP_FILE)
	@echo "StartupNotify=true" >> $(DESKTOP_FILE)
	@command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database $(APPDIR) || true
	@command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 >/dev/null 2>&1 || true
	@command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t $(PREFIX)/share/icons/hicolor >/dev/null 2>&1 || true
	@echo "Installation complete!"
	@echo "Executable path: $(BINDIR)/waveboostmanager"
	@echo "Desktop entry:   $(DESKTOP_FILE)"

uninstall:
	rm -f $(BINDIR)/waveboostmanager
	rm -f $(ICONDIR)/waveboostmanager.png
	rm -f $(DESKTOP_FILE)
	@command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database $(APPDIR) || true
	@command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 >/dev/null 2>&1 || true
	@command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t $(PREFIX)/share/icons/hicolor >/dev/null 2>&1 || true
	@echo "Uninstallation complete."

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned $(BUILD_DIR)/ directory."
