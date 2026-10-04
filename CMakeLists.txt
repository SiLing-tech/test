# ============================================================================
# CMakeLists.txt —— 光线投射位图渲染器的构建配置
#
# 目录约定：
#   include/   对外头文件
#   src/       实现（含 main.cpp）
#   build/     所有构建产物（中间文件、可执行文件、生成的位图都落在这里）
#   docs/      文档配图
#
# 用法（推荐直接用 build.ps1，它会把下面的命令都代劳）：
#   cmake -S . -B build
#   cmake --build build --config Release
#   ./build/bitmap_demo.exe
# ============================================================================

cmake_minimum_required(VERSION 3.20)

project(raycast_bitmap
        VERSION 1.0.0
        DESCRIPTION "用光线投射把 24 位 BMP 位图当作投影平面来渲染球体"
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ 标准
#   C++17 是硬性要求：代码里用到了 std::optional（object.h / raytracer.cpp）。
#   CXX_EXTENSIONS OFF 表示用 -std=c++17 而不是 -std=gnu++17，保证可移植。
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# 产物统一放进 build/，源码目录保持干净
#   ARCHIVE 静态库 / LIBRARY 动态库 / RUNTIME 可执行文件
# ---------------------------------------------------------------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 多配置生成器（Visual Studio）会把产物放在 bin/<Config>/ 下，
# 这里逐配置覆盖，让 exe 直接落在 build/bin/ 里，路径可预测。
foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# ---------------------------------------------------------------------------
# 源文件
#   把「除 main.cpp 以外的一切」编译成一个静态库 raycast_core，
#   可执行文件只含 main.cpp 并链接它。好处是以后要加测试或第二个程序时，
#   直接链接 raycast_core 即可，不必重复编译这些实现。
# ---------------------------------------------------------------------------
set(RAYCAST_CORE_SOURCES
    src/bitmap.cpp
    src/geometry.cpp
    src/object.cpp
    src/raytracer.cpp
    src/settings.cpp)

set(RAYCAST_HEADERS
    include/bitmap.h
    include/geometry.h
    include/object.h
    include/raytracer.h
    include/settings.h)

add_library(raycast_core STATIC ${RAYCAST_CORE_SOURCES} ${RAYCAST_HEADERS})

# 头文件在 include/ 下，所以源码里可以写 #include "bitmap.h" 这种短路径。
#   写成 PUBLIC 是因为可执行文件也会 include 这些头。
target_include_directories(raycast_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------------------
# 主程序
# ---------------------------------------------------------------------------
add_executable(bitmap_demo src/main.cpp)
target_link_libraries(bitmap_demo PRIVATE raycast_core)

# ---------------------------------------------------------------------------
# 编译器警告与编码
# ---------------------------------------------------------------------------
#   注意 /utf-8 是必需的：源码是 UTF-8 无 BOM，而简体中文 Windows 的默认
#   代码页是 936(GBK)。不给这个开关，源码里的中文注释和中文字符串会被按 GBK
#   解释，轻则乱码，重则触发 C4819 警告甚至编译失败。
if(MSVC)
    target_compile_options(raycast_core PUBLIC /W4 /utf-8)
    # 关掉 MSVC 对「C++ 标准库函数不安全」的告警（本项目没用到那些函数，
    # 但默认会定义 _CRT_SECURE_NO_WARNINGS 以外的提示）
    target_compile_definitions(raycast_core PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(raycast_core PUBLIC -Wall -Wextra -pedantic)
endif()

# ---------------------------------------------------------------------------
# 让 exe 运行时的工作目录就是 build/bin/
#   程序按相对路径输出 sphere.bmp，这样生成结果会落在 build/bin/sphere.bmp，
#   而不是跑到源码根目录去。
# ---------------------------------------------------------------------------
set_target_properties(bitmap_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
