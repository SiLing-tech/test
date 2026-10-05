# 手写图形渲染 · 教程代码

一本用 **C++17 标准库从零手写图形/渲染程序**的教程代码仓库，按章节组织。
不依赖任何第三方库 —— 连 BMP 文件的写入都是自己按字节拼出来的。

代码按「章节」分目录，跨章节复用的基础设施放在 `common/` 里，后续章节不必重复造轮子。

---

## 章节

| 章节 | 内容 | 状态 |
|---|---|---|
| [section_2](section_2/README.md) | **光线投射**：把 24 位 BMP 位图当作投影平面，摄像机位于平面中点正上方，每个像素发一条射线与场景中的球体求交 | ✅ 完成 |

![第 2 节渲染结果：橙色球体位于黑色背景中央](docs/sphere_preview.png)

---

## 快速开始

### 环境要求

| 项目 | 要求 |
|---|---|
| 编译器 | 支持 C++17（用到 `std::optional`）。已在 MSVC 19.51（Visual Studio 2026）上验证 |
| 构建工具 | CMake ≥ 3.20。Visual Studio 自带的 CMake 即可，**不需要**单独安装 |
| 操作系统 | 已在 Windows 10/11 验证；理论上 Linux/macOS 也能构建 |

### 构建与运行

在仓库**根目录**一条命令搞定（脚本会自动找到 Visual Studio 自带的 CMake）：

```powershell
.\build.ps1
```

它会配置并编译整个仓库，然后运行第 2 节的演示程序，输出位图到 `build/bin/sphere.bmp`。

> **如果提示「无法加载脚本，因为在此系统上禁止运行脚本」** —— 这是 PowerShell
> 执行策略的默认设置，不是脚本的问题。最省事的办法是不改系统设置，只对本次运行放行：
>
> ```powershell
> powershell -ExecutionPolicy Bypass -File .\build.ps1
> ```

脚本的其他用法：

```powershell
.\build.ps1 -List               # 列出已构建出来的可执行程序
.\build.ps1 -Target <名字>      # 运行指定的可执行程序（不带 .exe）
.\build.ps1 -Config Debug       # 构建 Debug 版
.\build.ps1 -NoRun              # 只构建，不运行
.\build.ps1 -Clean              # 先删掉 build/ 再从头配置
```

不用脚本的话（从根目录构建整个仓库）：

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\bin\section_2_demo.exe
```

> **用 VS Code / Visual Studio 打开**：根目录已有 `CMakeLists.txt`，
> 两个 IDE 都能直接识别并配置，不必手动敲命令。

---

## 目录结构

```
book/                         仓库根
├── CMakeLists.txt            根构建配置：定义公共库、挂载各章节
├── build.ps1                 一键构建脚本
├── .gitignore                排除构建产物
├── README.md                 本文档（中文）
├── README.en.md              English README
│
├── common/                   ★ 跨章节复用的公共代码
│   ├── CMakeLists.txt        定义静态库 book_common
│   ├── include/
│   │   ├── bitmap.h          一张内存中的 24 位 BMP 位图：填色、改像素、写盘
│   │   └── geometry.h        三维向量 Vector 与射线 Ray
│   └── src/
│       ├── bitmap.cpp
│       └── geometry.cpp
│
├── docs/                     文档配图
│   └── sphere_preview.png
│
├── section_2/                第 2 节：光线投射
│   ├── CMakeLists.txt
│   ├── README.md             本节详细文档（中文）
│   ├── README.en.md          English
│   ├── include/              object.h / raytracer.h / settings.h
│   └── src/                  object.cpp / raytracer.cpp / settings.cpp / main.cpp
│
└── build/                    构建产物（不进版本控制，可随时删除重建）
    ├── bin/section_2_demo.exe
    ├── bin/sphere.bmp        程序运行后生成
    ├── lib/book_common.lib
    └── lib/section_2_raytracing.lib
```

---

## 怎么加新章节

以加 `section_3` 为例，一共三步：

1. **建目录**：`section_3/include/`、`section_3/src/`
2. **写 `section_3/CMakeLists.txt`**，照 `section_2/CMakeLists.txt` 的样子改：
   定义本章的算法库与可执行程序，然后 `target_link_libraries(<目标> PUBLIC book_common)`
   即可获得公共库的头文件路径与编译选项。
3. **在根 `CMakeLists.txt` 末尾加一行**：

   ```cmake
   add_subdirectory(section_3)
   ```

根 `CMakeLists.txt` 刻意不定义任何 `add_executable` —— 可执行程序属于各自章节，
所以新增章节时根文件只需要多这一行。

### 章节可以单独构建

每个章节的 `CMakeLists.txt` 都能当独立项目用，不经过根目录：

```powershell
cmake -S section_2 -B build_section_2
cmake --build build_section_2 --config Release
```

因为 `section_2/CMakeLists.txt` 里有 `if(NOT TARGET book_common)` 的判断 ——
单独构建时它会自己把 `common/` 挂进来，从根目录构建时则不会重复定义。
两条路径编译出的可执行文件**逐字节等价**（已验证产物哈希完全相同）。

---

## 关于公共库 `common/`

只放「与具体渲染算法无关」的基础设施：

- **`Bitmap`** —— 一张内存中的 24 位 BMP 位图。BMP 格式里的文件头、像素的 BGR 顺序、
  每行末尾的 4 字节对齐、像素自下而上存储等细节，全部封装在类内部。
- **`Vector` / `Ray`** —— 三维向量与射线参数方程，通用几何基元。

判断标准很简单：**如果某样东西换成渲染算法也照样用得上，它就该放进 `common/`。**
反过来，第 2 节的 `Object` 抽象基类、`Raytracer`、`Settings` 都带有这一节的设计选择
（比如「只求交、不管涂色」的基类接口），所以留在章节里 —— 后续章节完全可以不采用那套设计。

依赖关系是严格单向的，没有循环包含：

```
common/  ──→  section_N/  ──→  main.cpp
```

`common/` 从不包含任何章节的头文件。

---

## 编码与构建注意事项

- **源码是 UTF-8 无 BOM，而简体中文 Windows 默认代码页是 936(GBK)**，
  所以 `common/CMakeLists.txt` 里给 MSVC 加了 `/utf-8`。这个开关标为 `PUBLIC`，
  所有链接 `book_common` 的章节自动继承，无需各自重复设置。**去掉它中文注释会乱码。**
- **`build.ps1` 刻意写成纯 ASCII**（英文提示与注释）。Windows PowerShell 5.1 读取
  `.ps1` 时，若文件没有 UTF-8 BOM 就按系统 ANSI 代码页解码，脚本里的中文会被打乱
  并导致语法错误。`.cpp` 可以用编译器开关解决，`.ps1` 不行。
- 编译始终开启最高警告级别（MSVC 的 `/W4`，GCC/Clang 的 `-Wall -Wextra -pedantic`）
  并保持**零警告**。

---

## 许可

教学示例代码，可自由使用与修改。
