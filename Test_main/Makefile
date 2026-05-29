#总体意思:自动搜索 ../common/*.cpp 和 test_*.cpp 自动生成test_xxx

#编译器是g++  $(CXX)就等价于g++
CXX      := g++

#编译选项使用C++14, -Wall打开常见警告 -Wextra打开更多警告
# $(CXXFLAGS) 就等价于 -std=c++14 -Wall -Wextra
CXXFLAGS := -std=c++14 -Wall -Wextra

#源码目录 即../common(放cpp文件)
SRCDIR   := ../common

# wildcard搜索匹配文件 保存所有 所有SRCDIR目录下的cpp文件列表
SRCS     := $(wildcard $(SRCDIR)/*.cpp)

# $(patsubst %.cpp,% , ...) 把.cpp去掉
# $(wildcard test_*.cpp) 搜索 test_xxx.cpp
# 意思TESTS就是去掉cpp后缀
TESTS    := $(patsubst %.cpp,%,$(wildcard test_*.cpp))

#声明 all single clean 不是文件，是命令,永远执行
#则,make all 永远执行
.PHONY: all single clean

# all 依赖于 $(TESTS) 里的所有目标
# make all 时会自动构建全部测试
all: $(TESTS)

# 编译单个测试：make single NAME=test_classfactory
single:
#如果没有定义 NAME
ifndef NAME
	#报错并停止
	$(error 请指定测试名，例如: make single NAME=test_classfactory)
endif
	#执行编译命令,与最上面的对应
	$(CXX) $(CXXFLAGS) $(SRCS) $(NAME).cpp -o $(NAME)
	
#模式规则,任何目标，都可以由同名 cpp 编译得到
%: %.cpp $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $< -o $@

# 把多个 cpp 编译并链接成最终程序
# g++ 源文件 编译选项 链接库 输出文件
main:
	g++ main.cpp \
		../Smw/utility/ClassFactory.cpp \
		../Smw/utility/Logger.cpp \
		../Smw/Sensor_manager.cpp \
		../IMU/WT61/WT61.cpp \
		../Speed/BRT38/BRT38.cpp \
		../Speed/OID-R5008D/OID-R5008D.cpp \
	-I../Smw/utility/ \
	-I../Smw/utility/ \
	-I../Smw/ \
	-I../IMU/ \
	-ludev -o main.out

# 删除编译生成的文件 make clean
clean:
	rm -f $(TESTS) *.out

