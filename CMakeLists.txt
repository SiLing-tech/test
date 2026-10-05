# ============================================================================
# CMakeLists.txt —— 仓库根构建配置
#
# 这个仓库是一本「用 C++ 手写图形/渲染程序」的教程，按章节目录组织：
#
#   common/           跨章节复用的公共代码（不依赖任何章节）
#     include/          对外头文件
#     src/              实现
#   section_2/        第 2 节：光线投射把位图当投影平面渲染球体
#     include/          本节专属头文件
#     src/              本节实现（含 main.cpp）
#   build/            所有构建产物（中间文件、静态库、可执行文件）
#   docs/             文档配图
#
# 根 CMakeLists 只做两件事：定义公共库、把各章节挂进来。
# 每个章节自己有一个 CMakeLists.txt，也可以单独进入该目录构建。
#
# 用法（推荐直接用 build.ps1，它会把下面的命令都代劳）：
#   cmake -S . -B build
#   cmake --build build --config Release
#   ./build/bin/section_2_demo.exe
#
# 本文件刻意不定义任何 add_executable：可执行程序属于各自章节。
# 这样以后新增章节时，根文件只需要多一行 add_subdirectory()。
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# 只有在「本文件就是顶层项目」时才调用 project()。
#   project() 在非顶层位置调用是非法的。从根目录构建时本文件必然是顶层；
#   而当某个章节被单独构建、并把 common/ 挂进来时本文件根本不会被执行，
#   所以这个保护更多是防止将来有人误从别处 add_subdirectory 本目录。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(book
            VERSION 1.0.0
            DESCRIPTION "手写图形/渲染程序的教程代码，按章节组织"
            LANGUAGES CXX)
endif()

# ---------------------------------------------------------------------------
# C++ 标准
#   C++17 是硬性要求：代码里用到了 std::optional（物体求交的返回值）。
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

# 多配置生成器（Visual Studio）会把产物放在 <type>/<Config>/ 下，
# 这里逐配置覆盖，让 exe 与库直接落在 build/bin、build/lib 里，路径可预测。
foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/lib")
endforeach()

# ---------------------------------------------------------------------------
# 公共库 book_common
#   定义在 common/CMakeLists.txt 里，这里只负责把它挂进来。
#   公共库自身的说明（目前含 Bitmap 与 Vector/Ray）见该文件。
#
#   为什么不在本文件里直接定义：common/ 需要能被「单独构建某个章节」的
#   场景直接挂载（那时根 CMakeLists 不参与），所以它必须自包含。
#   定义只有一处，避免两处写法不一致。
# ---------------------------------------------------------------------------
add_subdirectory(common)

# ---------------------------------------------------------------------------
# 各章节
#   想加新章节，就在这里再加一行 add_subdirectory(section_3) 即可。
#
#   if(NOT TARGET ...) 是配合「单独构建某个章节」用的：那种情况下本文件是被
#   章节的 CMakeLists 通过 add_subdirectory 挂进来的，目标早已定义好，
#   这里就不该再定义一次（否则会报目标重复）。从根目录正常构建时两者都成立，
#   行为不变。
# ---------------------------------------------------------------------------
if(NOT TARGET section_2_demo)
    add_subdirectory(section_2)
endif()
