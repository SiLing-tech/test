# ============================================================================
# common/CMakeLists.txt —— 跨章节复用的公共库
#
# 目前只有两样东西，都是「与具体渲染算法无关」的基础设施：
#   Bitmap        —— 一张内存中的 24 位 BMP 位图：填色、改像素、写盘
#   Vector / Ray  —— 三维向量与射线，通用几何基元
#
# 它们不依赖任何章节，所以后续章节都可以直接复用，不必各自抄一份位图读写代码。
#
# 用法：
#   在本目录已经处于某个 CMake 项目树中时（根目录已 add_subdirectory(common)），
#   章节只需要 target_link_libraries(<你的目标> PUBLIC book_common) 即可，
#   头文件路径与编译选项都会自动继承。
#
#   本文件不含 project() 调用，这是刻意的 —— 这样它可以在任何位置被
#   add_subdirectory 挂进来，包括「单独构建某个章节」时由章节来挂载。
# ============================================================================

add_library(book_common STATIC
    src/bitmap.cpp
    src/geometry.cpp)

# 头文件都在本目录的 include/ 下。
# 写成 PUBLIC：链接本库的目标（各章节）会自动获得这个搜索路径，
# 所以章节源码里直接写 #include "bitmap.h" 即可，不必重复设置。
# 用 CMAKE_CURRENT_SOURCE_DIR 而不是拼相对路径，这样无论本目录被挂到
# 项目树的哪一层，路径都是对的。
target_include_directories(book_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---------------------------------------------------------------------------
# 编译器警告与编码
# ---------------------------------------------------------------------------
#   注意 /utf-8 是必需的：源码是 UTF-8 无 BOM，而简体中文 Windows 的默认
#   代码页是 936(GBK)。不给这个开关，源码里的中文注释和中文字符串会被按 GBK
#   解释，轻则乱码，重则触发警告甚至编译失败。
#   标为 PUBLIC，让所有链接本库的章节自动继承，无需各自重复设置。
if(MSVC)
    target_compile_options(book_common PUBLIC /W4 /utf-8)
    # 关掉 MSVC 对「C++ 标准库函数不安全」的告警
    target_compile_definitions(book_common PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(book_common PUBLIC -Wall -Wextra -pedantic)
endif()
