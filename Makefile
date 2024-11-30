CC = gcc


VCPKG_DIR = C:/progs/vcpkg/installed/x64-windows
WIN_KITS_DIR = C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0

CFLAGS = -Wall -I./src -I"$(VCPKG_DIR)/include" -I"$(WIN_KITS_DIR)/shared" -I"$(WIN_KITS_DIR)/um"
LDFLAGS = -L"$(VCPKG_DIR)/lib" -lopengl32 -lglfw3dll -lglew32 -lm -lfreeglut

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_FILE = build.log

SOURCES = $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = $(BIN_DIR)/minecraft_clone.exe

all: copy_assets $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) > $(LOG_FILE) 2>&1

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@ >> $(LOG_FILE) 2>&1

run: $(EXECUTABLE)
	$(EXECUTABLE)

ASSET_DIR = $(SRC_DIR)/assets
BIN_ASSET_DIR = $(BIN_DIR)/assets
DLLS_TO_COPY = freeglut.dll glew32.dll glfw3.dll

copy_assets:
	@if not exist "$(BIN_ASSET_DIR)" mkdir "$(BIN_ASSET_DIR)"
	@xcopy /E /I /Y "$(ASSET_DIR)" "$(BIN_ASSET_DIR)\"
	for %%i in ($(DLLS_TO_COPY)) do copy /Y "$(VCPKG_DIR)\bin\%%i" "$(BIN_DIR)\"
	
clean:
	rmdir /s /q $(OBJ_DIR)
	rmdir /s /q $(BIN_DIR)
	del /q $(LOG_FILE)

.PHONY:  all  clean run