CC = gcc

ifeq ($(OS),Windows_NT)
	LIBRARY_DIR = C:/Progs/vcpkg/installed/x64-windows
	WIN_KITS_DIR = C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0

	CFLAGS = -Wall -I./src -I"$(LIBRARY_DIR)/include" -I"$(WIN_KITS_DIR)/shared" -I"$(WIN_KITS_DIR)/um"
	LDFLAGS = -L"$(LIBRARY_DIR)/lib" -lopengl32 -lglfw3dll -lglew32 -lm -lfreeglut

	EXECUTABLE = $(BIN_DIR)/minecraft_clone.exe

	CREATE_BIN_DIR = @if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	CREATE_ASSET_DIR = @if not exist "$(BIN_ASSET_DIR)" mkdir "$(BIN_ASSET_DIR)"
	CREATE_SUBDIR = @if not exist "$(dir $@)" mkdir "$(dir $@)"

	COPY_ASSET_DIR = @xcopy /E /I /Y "$(ASSET_DIR)" "$(BIN_ASSET_DIR)\"

	DLLS_TO_COPY = freeglut.dll glew32.dll glfw3.dll

	CLEAN_OBJ = rmdir /s /q $(OBJ_DIR)
	CLEAN_BIN = rmdir /s /q $(BIN_DIR)
	CLEAN_LOG = del /q $(LOG_FILE)
else
	CFLAGS = -Wall -I./src
	LDFLAGS = -lGL -lglfw -lGLEW -lm -lglut

	EXECUTABLE = $(BIN_DIR)/minecraft_clone

	CREATE_BIN_DIR = @mkdir -p $(BIN_DIR)
	CREATE_ASSET_DIR = @mkdir -p $(BIN_ASSET_DIR)
	CREATE_SUBDIR = @mkdir -p $(dir $@)

	COPY_ASSET_DIR = cp -r $(ASSET_DIR) $(BIN_DIR)/

	CLEAN_OBJ = rm -rf $(OBJ_DIR)
	CLEAN_BIN = rm -rf $(BIN_DIR)
	CLEAN_LOG = rm -f $(LOG_FILE)
endif

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
ASSET_DIR = $(SRC_DIR)/assets
BIN_ASSET_DIR = $(BIN_DIR)/assets
LOG_FILE = build.log

SOURCES = $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

all: $(EXECUTABLE) copy_assets

$(EXECUTABLE): $(OBJECTS)
	$(CREATE_BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) > $(LOG_FILE) 2>&1
	@echo "Build completed. Executable: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CREATE_SUBDIR)
	$(CC) $(CFLAGS) -c $< -o $@ >> $(LOG_FILE) 2>&1

copy_assets:
	@mkdir -p $(BIN_ASSET_DIR)
	cp -r $(ASSET_DIR) $(BIN_DIR)/
ifeq ($(OS),Windows_NT)
	@for %%i in ($(DLLS_TO_COPY)) do copy /Y "$(LIBRARY_DIR)\bin\%%i" "$(BIN_DIR)\"
endif
	@echo "Assets copied to: $(BIN_ASSET_DIR)"

run: $(EXECUTABLE) copy_assets
	@echo "Running $(EXECUTABLE)..."
	cd $(BIN_DIR) && ./$(notdir $(EXECUTABLE))

clean:
	$(CLEAN_OBJ)
	$(CLEAN_BIN)
	$(CLEAN_LOG)
	@echo "Clean completed."

.PHONY: all clean run copy_assetss