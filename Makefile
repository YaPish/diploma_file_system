# -------------------------------
# Project Settings
# -------------------------------
TARGET      = fs

# Toolchain
TOOLCHAIN   = /usr/bin/
CC          = $(TOOLCHAIN)gcc

# Directories
SRC_DIR     = src
INC_DIR     = include
BUILD_DIR   = build
BIN_DIR     = bin

# Files
C_SRCS      = $(wildcard $(SRC_DIR)/*.c)

# -------------------------------
# Compiler/Linker Flags
# -------------------------------
CFLAGS      = -O0 -g -Wall -I$(INC_DIR)

# -------------------------------
# Build Rules
# -------------------------------
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SRCS:.c=.o)))

all: $(BUILD_DIR) $(BIN_DIR) $(BIN_DIR)/$(TARGET)  # Явное создание директорий

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -MMD -o $@ $<  # Добавлен -MMD

$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

# -------------------------------
# Utilities
# -------------------------------
run:
	./$(BIN_DIR)/$(TARGET)

gdb:
	lldb ./$(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)  # Теперь .d файлы будут генерироваться

.PHONY: all run clean gdb  # Добавлен gdb
