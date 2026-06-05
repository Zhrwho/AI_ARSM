# 编译器
CXX      := g++
CXXFLAGS := -std=c++14 -Wall -Wextra
LDFLAGS  := -pthread

# 动态库名
LIB_NAME := libmysmw.so

# 源文件目录
UTILITY_DIR  := ../Smw/utility
SMW_DIR      := ../Smw
IMU_DIR      := ../IMU
SPEED_DIR    := ../Speed
UTIL_TEST_DIR := $(UTILITY_DIR)/Test

# 自动收集源文件（排除 Test 目录）
UTILITY_SRCS := $(filter-out $(UTILITY_DIR)/Test/%,$(wildcard $(UTILITY_DIR)/*.cpp))
SMW_SRCS     := $(filter-out $(SMW_DIR)/Test/%,$(wildcard $(SMW_DIR)/*.cpp))
IMU_SRCS     := $(wildcard $(IMU_DIR)/**/*.cpp)
SPEED_SRCS   := $(wildcard $(SPEED_DIR)/**/*.cpp)

# 合并所有源文件
ALL_COMMON_SRCS := $(UTILITY_SRCS) $(SMW_SRCS) $(IMU_SRCS) $(SPEED_SRCS)

# 包含目录
INCLUDES := -I$(UTILITY_DIR) -I$(SMW_DIR) -I$(IMU_DIR)

# 让可执行文件在同级目录找到 .so（不需要设 LD_LIBRARY_PATH）
RPATH := -Wl,-rpath,'$$ORIGIN'

# 动态库链接参数
# --no-as-needed 确保 .so 始终被加载，工厂注册的静态全局对象才能执行
LIB_LINK := -Wl,--no-as-needed -L. -lmysmw -Wl,--as-needed

# 自动收集 utility/Test 下的测试程序
UTIL_TESTS := $(basename $(wildcard $(UTIL_TEST_DIR)/*_test.cpp))

.PHONY: all lib main test_main test_subloop test_framework utility_tests clean

all: lib main

# ==================== 动态库 ====================
lib: $(LIB_NAME)

$(LIB_NAME): $(ALL_COMMON_SRCS)
	$(CXX) $(CXXFLAGS) -shared -fPIC $^ $(INCLUDES) $(LDFLAGS) -ludev -o $@
	@echo ">>> $(LIB_NAME) 编译完成"

# ==================== 主程序（链接动态库） ====================
main: main.cpp $(LIB_NAME)
	$(CXX) $(CXXFLAGS) main.cpp $(INCLUDES) $(LDFLAGS) $(LIB_LINK) $(RPATH) -o main.out
	@echo ">>> main.out 编译完成"

# ==================== 测试：真实传感器插拔 ====================
test_main: test_main.cpp $(LIB_NAME)
	$(CXX) $(CXXFLAGS) test_main.cpp $(INCLUDES) $(LDFLAGS) $(LIB_LINK) $(RPATH) -o test_main.out

# ==================== 测试：EventLoop + select ====================
test_subloop: test_subloop.cpp $(LIB_NAME)
	$(CXX) $(CXXFLAGS) test_subloop.cpp $(INCLUDES) $(LDFLAGS) $(LIB_LINK) $(RPATH) -o test_subloop.out

# ==================== 完整测试框架 ====================
test_framework: test_framework.cpp $(LIB_NAME)
	$(CXX) $(CXXFLAGS) test_framework.cpp $(INCLUDES) $(LDFLAGS) $(LIB_LINK) $(RPATH) -ludev -o test_framework.out

# ==================== utility/Test 下的独立测试 ====================
utility_tests: $(UTIL_TESTS)

# utility/Test 下的测试程序（静态编译，不依赖动态库）
$(UTIL_TEST_DIR)/%: $(UTIL_TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $< $(UTILITY_SRCS) $(INCLUDES) $(LDFLAGS) -o $@

# ==================== 清理 ====================
clean:
	rm -f *.out
	rm -f *.so
	rm -f $(UTIL_TEST_DIR)/*_test
