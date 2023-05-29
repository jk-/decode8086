CC        := clang
CFLAGS    := -Wall -Wextra -g
APP_NAME  := main.out
BUILD_DIR := build

# build/main.out
APP := $(BUILD_DIR)/$(APP_NAME)
OBJ := $(wildcard *.c)
OBJ := $(OBJ:%.c=%.o)
OBJ := $(addprefix $(BUILD_DIR)/,$(OBJ))

.PHONY: all target compile clean

all: compile

compile: clean target

target: build_dir $(APP)

$(APP): $(OBJ)
	$(CC) $^ -o $@

build_dir:
	@-mkdir $(BUILD_DIR) 2>/dev/null || true

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo -n "Removing build files..."
	@-rm -rf $(BUILD_DIR)
	@echo "Done!"
