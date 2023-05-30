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


#
# TESTING
#

TEST_DIR     := tests
# list of all asm files to test
TEST_ASM     := $(wildcard ${TEST_DIR}/*.asm)
# dir where temporary test files will be placed
TEST_OUT_DIR := $(BUILD_DIR)/$(TEST_DIR)
# tests/0001.asm ==> build/tests/0001.asm
TEST_OUT_ASM := $(addprefix ${BUILD_DIR}/,${TEST_ASM})

# tests/0001.asm
TEST_ASM_ORIG_SRC := $(TEST_ASM)
# build/tests/0001.asm.out
TEST_ASM_ORIG_OBJ := $(addsuffix .out,${TEST_OUT_ASM})
# build/tests/0001.asm.gen
TEST_ASM_GEN_SRC  := $(addsuffix .gen,${TEST_OUT_ASM})
# build/tests/0001.asm.gen.out
TEST_ASM_GEN_OBJ  := $(addsuffix .gen.out,${TEST_OUT_ASM})

.PHONY: test test_build_dir compare

test: test_build_dir compare

test_build_dir: build_dir
	@-mkdir -p $(TEST_OUT_DIR) 2>/dev/null || true

# tests/0001.asm ==> build/tests/0001.asm.out
$(TEST_ASM_ORIG_OBJ): $(TEST_OUT_DIR)/%.asm.out: $(TEST_DIR)/%.asm
	@nasm $< -o $@

# build/tests/0001.asm.out ==> build/tests/0001.asm.gen
$(TEST_ASM_GEN_SRC): %.gen: %.out $(APP)
	@./$(APP) $< > $@

# build/tests/0001.asm.gen ==> build/tests/0001.asm.gen.out
$(TEST_ASM_GEN_OBJ): %.gen.out: %.gen
	@nasm $< -o $@

compare: cmp.sh $(TEST_ASM_GEN_OBJ) $(TEST_ASM_ORIG_OJB)
	@for file in $(TEST_OUT_ASM); do \
		./$< $$file.out $$file.gen.out; \
	done
