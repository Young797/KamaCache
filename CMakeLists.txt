# 设置最低 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(KCacheSystem)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 指定源文件目录下的所有 .cpp 文件
file(GLOB SOURCES "*.cpp")

# 设置目标可执行文件
# add_executable(main ${SOURCES})
add_executable(test test.cpp)
add_executable(testAllCachePolicy testAllCachePolicy.cpp)

# 清理中间的 .o 文件
# set_target_properties(main PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(test PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(testAllCachePolicy PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# 额外的编译选项（可根据需要启用）
# target_compile_options(main PRIVATE -Wall -Wextra -O2)