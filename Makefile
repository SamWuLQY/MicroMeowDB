# MicroMeowDB Makefile

# 检测操作系统
ifeq ($(OS),Windows_NT)
    # Windows平台
    CC = gcc
    CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I./src
    LDFLAGS = -lpthread -lm -lws2_32
    RM = del /Q /F
    MKDIR = mkdir
    INSTALL_DIR = C:\Program Files\MicroMeowDB\bin
else
    # Linux/Unix平台
    CC = gcc
    CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I./src
    LDFLAGS = -lpthread -ldl -lm
    RM = rm -rf
    MKDIR = mkdir -p
    INSTALL_DIR = /usr/local/bin
endif

# 目录设置
SRC_DIR = ./src
TEST_DIR = ./tests
OBJ_DIR = ./obj
BIN_DIR = ./bin

# 源文件
SRC_FILES = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/config/config.c \
    $(SRC_DIR)/error/error.c \
    $(SRC_DIR)/logging/logging.c \
    $(SRC_DIR)/memory/memory_pool.c \
    $(SRC_DIR)/memory/memory_cache.c \
    $(SRC_DIR)/storage/storage_engine.c \
    $(SRC_DIR)/storage/row_engine.c \
    $(SRC_DIR)/storage/column_engine.c \
    $(SRC_DIR)/storage/memory_engine.c \
    $(SRC_DIR)/index/b_plus_tree.c \
    $(SRC_DIR)/index/lsm_tree.c \
    $(SRC_DIR)/index/hash_index.c \
    $(SRC_DIR)/index/r_tree.c \
    $(SRC_DIR)/index/bloom_filter.c \
    $(SRC_DIR)/index/bitmap_index.c \
    $(SRC_DIR)/security/authentication.c \
    $(SRC_DIR)/security/authorization.c \
    $(SRC_DIR)/network/network.c \
    $(SRC_DIR)/transaction/transaction.c \
    $(SRC_DIR)/monitoring/monitoring.c \
    $(SRC_DIR)/backup/backup.c \
    $(SRC_DIR)/metadata/metadata.c \
    $(SRC_DIR)/audit/audit.c \
    $(SRC_DIR)/resource/resource.c \
    $(SRC_DIR)/optimizer/optimizer.c \
    $(SRC_DIR)/procedure/procedure.c \
    $(SRC_DIR)/replication/replication.c \
    $(SRC_DIR)/system/system.c \
    $(SRC_DIR)/client/client.c \
    $(SRC_DIR)/util/path.c

# 测试文件
TEST_FILES = \
    $(TEST_DIR)/main.c \
    $(TEST_DIR)/test.c

# 目标文件
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJ_FILES = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_FILES))

# 可执行文件
ifeq ($(OS),Windows_NT)
    TARGET = $(BIN_DIR)/micromeowdb.exe
    TEST_TARGET = $(BIN_DIR)/test_micromeowdb.exe
else
    TARGET = $(BIN_DIR)/micromeowdb
    TEST_TARGET = $(BIN_DIR)/test_micromeowdb
endif

# 规则
all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(OBJ_FILES)
	@$(MKDIR) $(BIN_DIR) 2>/dev/null || true
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_TARGET): $(filter-out $(OBJ_DIR)/main.o,$(OBJ_FILES)) $(TEST_OBJ_FILES)
	@$(MKDIR) $(BIN_DIR) 2>/dev/null || true
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR) $(dir $@) 2>/dev/null || true
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@$(MKDIR) $(dir $@) 2>/dev/null || true
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@$(RM) $(OBJ_DIR) $(BIN_DIR) 2>/dev/null || true

run: $(TARGET)
	$(TARGET)

run_test: $(TEST_TARGET)
	$(TEST_TARGET)

# 安装规则
install: $(TARGET)
	@echo "Installing MicroMeowDB..."
	@$(MKDIR) $(INSTALL_DIR) 2>/dev/null || true
	@cp $(TARGET) $(INSTALL_DIR)/
	@echo "MicroMeowDB installed successfully!"

uninstall:
	@echo "Uninstalling MicroMeowDB..."
	@$(RM) $(INSTALL_DIR)/micromeowdb* 2>/dev/null || true
	@echo "MicroMeowDB uninstalled successfully!"

# 依赖项
.PHONY: all clean run run_test install uninstall
