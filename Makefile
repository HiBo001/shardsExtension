# Compiler and Linker
CXX = g++
CXXFLAGS = -g -Wall -std=c++11
LDFLAGS = -pthread

# Directory settings
SOURCE_DIR = source
BUILD_DIR = build

# Source files (指定在 source 目录下)
SRCS = $(SOURCE_DIR)/main.cpp $(SOURCE_DIR)/common.cpp $(SOURCE_DIR)/shardsExtension.cpp $(SOURCE_DIR)/tpsModel.cpp

# Object files (从 source/xxx.cpp 映射为 build/xxx.o)
# notdir 去掉路径前缀，再用 addprefix 加上 build/
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(SRCS:.cpp=.o)))

# Output executable
EXEC = $(BUILD_DIR)/shardsExtension

# Default target
all: $(BUILD_DIR) $(EXEC)

# Create the build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link the object files into the final executable
$(EXEC): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(EXEC)

# Rule for compiling .cpp files into .o object files
# 使用 vpath 让 Make 知道去 source 目录找源文件
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(SOURCE_DIR)/common.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean