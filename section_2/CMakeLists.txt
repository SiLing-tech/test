# ============================================================================
# section_2/CMakeLists.txt —— 第 2 节：光线投射
#
# 本节做的事：把一张 24 位 BMP 位图当作投影平面，摄像机放在平面中点正上方，
# 对每个像素发一条射线，与场景里的物体求交，命中就涂物体色、否则涂背景色。
#
# 本节代码分成两层：
#   section_2_raytracing  本节自己的算法（参数配置 / 物体 / 渲染器）
#   section_2_demo        可执行程序，只含 main.cpp
# 这样拆分是为了以后写测试时能直接链接算法库，不必重复编译。
#
# 依赖：公共库 book_common（位图读写 + 向量/射线）
#
# 单独构建本节（不经过根目录，即把本目录当成一个独立项目）也是可以的：
#   cmake -S section_2 -B build_section_2
#   cmake --build build_section_2 --config Release
# 下面几个 if(CMAKE_SOURCE_DIR STREQUAL ...) 与 if(NOT TARGET ...) 就是为这条
# 路径准备的。日常使用建议直接从根目录构建（跑仓库根的 build.ps1），
# 那样产物路径统一落在 build/ 下，更可预测。
# ============================================================================

# ---------------------------------------------------------------------------
# 顶层项目声明（只在单独构建本目录时才需要）
#   cmake_minimum_required 无条件写：CMake 不要求它只出现在顶层。
#   project() 则必须判断 —— 从根目录构建时本文件是被 add_subdirectory 挂进来的，
#   而非顶层位置调用 project() 是非法操作。这里顺带把 C++17 设定也补上，
#   因为单独构建时根 CMakeLists 的 CMAKE_CXX_STANDARD 设置不会被执行。
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(section_2_raycast
            VERSION 1.0.0
            DESCRIPTION "第 2 节：用光线投射把位图当投影平面渲染球体"
            LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# ---------------------------------------------------------------------------
# 单独构建本目录时的产物落点与编译选项
#   CMAKE_SOURCE_DIR 与 CMAKE_CURRENT_SOURCE_DIR 相等，说明本目录就是顶层项目。
#   这时根 CMakeLists 的输出目录设置不生效（它没被执行），所以自己设一份，
#   让产物落在 build_section_2/bin 与 build_section_2/lib 下。
#   从根目录构建时本段整体跳过，产物仍统一落在 build/bin 与 build/lib。
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/lib")
    endforeach()
endif()

# ---------------------------------------------------------------------------
# 单独构建本目录时，先把公共库挂进来。
#   只挂 common/，不要挂上级目录：上级目录里的根 CMakeLists 含 project() 调用，
#   而 project() 在非顶层位置调用是非法的（CMake 会直接报错）。
#   common/ 本身不含 project()，只定义库，所以可以安全地作为子目录加入。
# ---------------------------------------------------------------------------
if(NOT TARGET book_common)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common"
                     "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

# ---------------------------------------------------------------------------
# 本节算法库
# ---------------------------------------------------------------------------
add_library(section_2_raytracing STATIC
    src/settings.cpp
    src/object.cpp
    src/raytracer.cpp)

target_include_directories(section_2_raytracing PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 链接公共库。写成 PUBLIC，是为了让链接本库的目标也自动继承
# book_common 的头文件路径（common/include）和编译选项（/utf-8、/W4 等）。
target_link_libraries(section_2_raytracing PUBLIC book_common)

# ---------------------------------------------------------------------------
# 本节的可执行程序
# ---------------------------------------------------------------------------
add_executable(section_2_demo src/main.cpp)
target_link_libraries(section_2_demo PRIVATE section_2_raytracing)

# ---------------------------------------------------------------------------
# 让程序运行时的工作目录就是 build/bin/
#   程序按相对路径输出 sphere.bmp，这样生成结果会落在 build/bin/sphere.bmp，
#   而不是跑到源码目录里。
# ---------------------------------------------------------------------------
set_target_properties(section_2_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
