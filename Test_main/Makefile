#编译器
CXX      := g++
CXXFLAGS := -std=c++14 -Wall -Wextra
LDFLAGS  := -pthread

# 源文件目录
UTILITY_DIR := ../Smw/utility

# 公共源文件
COMMON_SRCS := \
	$(UTILITY_DIR)/Logger.cpp \
	$(UTILITY_DIR)/CurrentThread.cpp \
	$(UTILITY_DIR)/Thread.cpp \
	$(UTILITY_DIR)/EventLoop.cpp \
	$(UTILITY_DIR)/EventLoopThread.cpp \
	$(UTILITY_DIR)/EventLoopThreadPool.cpp

.PHONY: all main test_main test_subloop test_framework clean

all: main

# 主程序
main:
	$(CXX) $(CXXFLAGS) main.cpp \
		$(UTILITY_DIR)/ClassFactory.cpp \
		$(COMMON_SRCS) \
		../Smw/Sensor_manager.cpp \
		../IMU/WT61/WT61.cpp \
		../Speed/BRT38/BRT38.cpp \
		../Speed/OID-R5008D/OID-R5008D.cpp \
		-I$(UTILITY_DIR) \
		-I../Smw/ \
		-I../IMU/ \
		$(LDFLAGS) -ludev -o main.out

# 测试真实传感器插拔
test_main:
	$(CXX) $(CXXFLAGS) test_main.cpp \
		$(UTILITY_DIR)/ClassFactory.cpp \
		$(COMMON_SRCS) \
		../Smw/Sensor_manager.cpp \
		../IMU/WT61/WT61.cpp \
		../Speed/BRT38/BRT38.cpp \
		../Speed/OID-R5008D/OID-R5008D.cpp \
		-I$(UTILITY_DIR) \
		-I../Smw/ \
		-I../IMU/ \
		$(LDFLAGS) -ludev -o test_main.out

# 测试 EventLoop + select
test_subloop:
	$(CXX) $(CXXFLAGS) test_subloop.cpp \
		$(COMMON_SRCS) \
		-I$(UTILITY_DIR) \
		-I../Smw/ \
		$(LDFLAGS) -o test_subloop.out

# 完整的测试框架（单元测试+集成测试）
test_framework:
	$(CXX) $(CXXFLAGS) test_framework.cpp \
		$(UTILITY_DIR)/ClassFactory.cpp \
		$(COMMON_SRCS) \
		../Smw/Sensor_manager.cpp \
		../IMU/WT61/WT61.cpp \
		../Speed/BRT38/BRT38.cpp \
		../Speed/OID-R5008D/OID-R5008D.cpp \
		-I$(UTILITY_DIR) \
		-I../Smw/ \
		-I../IMU/ \
		$(LDFLAGS) -ludev -o test_framework.out

# 删除编译生成的文件
clean:
	rm -f *.out