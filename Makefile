# Compiler and Linker
CXX = g++
CXXFLAGS = -g -Wall -std=c++11
LDFLAGS = -pthread

# Directory settings
SOURCE_DIR = source
BUILD_DIR = build
# 调整：bin 和 obj 现在平级，都在 build 下
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SRCS = $(SOURCE_DIR)/main.cpp $(SOURCE_DIR)/common.cpp $(SOURCE_DIR)/shardsExtension.cpp $(SOURCE_DIR)/tpsModel.cpp

# Object files (映射到 build/obj/)
OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS:.cpp=.o)))

# Output executable (映射到 build/bin/)
EXEC = $(BIN_DIR)/shardsExtension

# Default target
all: $(EXEC)

# 确保目录存在的规则
# 使用一行命令创建两个目录
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# 链接规则：依赖于 OBJS，且确保 BIN_DIR 存在
$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(EXEC)

# 编译规则：从 source/ 编译到 build/obj/，确保 OBJ_DIR 存在
$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(SOURCE_DIR)/common.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean