# 光线投射位图渲染器

用 **光线投射（ray casting）** 把一张 **24 位 BMP 位图**当作投影平面，渲染出球体。
纯 C++17 标准库实现，不依赖任何第三方库 —— 连 BMP 文件的写入都是自己按字节拼出来的。

![渲染结果：橙色球体位于黑色背景中央](docs/sphere_preview.png)

上图是程序的实际输出：640×480，球心 `(0, 0, 800)`、半径 `150`、颜色 `RGB(255, 80, 0)`，
摄像机位于世界原点、距投影平面 `D = 500`。

---

## 目录

- [它做了什么](#它做了什么)
- [快速开始](#快速开始)
- [目录结构](#目录结构)
- [几何设定](#几何设定)
- [代码结构与职责划分](#代码结构与职责划分)
- [怎么改参数](#怎么改参数)
- [怎么加新形状](#怎么加新形状)
- [数学原理](#数学原理)
- [BMP 输出格式](#bmp-输出格式)
- [验证过的正确性](#验证过的正确性)
- [已知限制](#已知限制)

---

## 它做了什么

对位图上的**每一个像素**：

1. 用像素坐标算出它在投影平面上的世界坐标 `P`；
2. 从摄像机（世界原点 `O`）向 `P` 发一条射线，方向 `d = P - O`；
3. 依次问场景里每个物体「这条射线和你交在哪里」；
4. 取**最近**的那个交点所属物体的颜色，涂到该像素上；都没命中就涂背景色。

整个过程没有任何光照模型 —— 只按判别式区分「打中」和「没打中」，所以画出来是**纯色剪影**。
`delta >= 0` 就算命中，这一条约定来自下面的[数学原理](#数学原理)。

---

## 快速开始

### 环境要求

| 项目 | 要求 |
|---|---|
| 编译器 | 支持 C++17（用到 `std::optional`）。已在 MSVC 19.51（Visual Studio 2026）上验证 |
| 构建工具 | CMake ≥ 3.20。Visual Studio 自带的 CMake 即可，**不需要**单独安装 |
| 操作系统 | 已在 Windows 10/11 验证；理论上 Linux/macOS 也能构建（CMakeLists 里已分别处理 MSVC 与 GCC/Clang 的编译选项） |

> **用 VS Code / Visual Studio 打开本项目**：仓库根目录已有 `CMakeLists.txt`，
> 两个 IDE 都能直接识别并配置，不必手动敲命令。

### 构建与运行

**在仓库根目录**一条命令搞定（脚本会自动找到 Visual Studio 自带的 CMake）：

```powershell
.\build.ps1
```

它会配置并编译**整个仓库**（公共库 + 本节），然后运行本节程序。看到类似输出即为成功：

```
已生成位图：sphere.bmp  640 x 480
摄像机 O(0,0,0)，投影平面 z = 500
背景色 RGB(0, 0, 0)
场景物体数：1
  球 #0  球心 (0, 0, 800)  R = 150  体积 = 1.41372e+07  颜色 RGB(255, 80, 0)
        命中圆半径约 95.4427 像素
```

生成的位图在 `build/bin/sphere.bmp`。

脚本的其他用法：

```powershell
.\build.ps1 -List               # 列出已构建出来的可执行程序
.\build.ps1 -Target <名字>      # 运行指定的可执行程序（不带 .exe）
```

<details>
<summary><b>如果提示「无法加载脚本，因为在此系统上禁止运行脚本」</b></summary>

这是 PowerShell 的执行策略（ExecutionPolicy）默认设置，不是脚本本身的问题。三种解决办法，任选其一：

```powershell
# 1. 只对本次运行放行（最省事，不改系统设置）
powershell -ExecutionPolicy Bypass -File .\build.ps1

# 2. 只对当前用户放开（一次性，长期有效）
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# 3. 干脆不用脚本，手动敲 CMake 命令
```

</details>

### 不用脚本的话

**方式一：从仓库根目录构建整个仓库**（推荐，产物路径统一）

```powershell
cd <仓库根>
cmake -S . -B build
cmake --build build --config Release
.\build\bin\section_2_demo.exe
```

**方式二：只构建本节**（把本节当一个独立项目）

```powershell
cmake -S section_2 -B build_section_2
cmake --build build_section_2 --config Release
.\build_section_2\bin\section_2_demo.exe
```

这一路径下 `section_2/CMakeLists.txt` 会自己把 `common/` 挂进来，产物落在
`build_section_2/` 里。两条路径编译出的可执行文件**逐字节等价**，产物哈希完全相同。

### 其他常用选项

```powershell
.\build.ps1 -Config Debug      # 构建 Debug 版
.\build.ps1 -NoRun             # 只构建，不运行
.\build.ps1 -Clean             # 先删掉 build/ 再从头配置
```

---

## 目录结构

本节是仓库中的一个章节。**公共代码放在仓库根的 `common/` 下**，供后续章节复用：

```
book/                         仓库根
├── CMakeLists.txt            根构建配置：只定义公共库、挂载各章节
├── build.ps1                 一键构建脚本（定位 CMake → 配置 → 编译 → 运行）
├── .gitignore                排除构建产物
│
├── common/                   ★ 跨章节复用的公共代码
│   ├── CMakeLists.txt        定义静态库 book_common
│   ├── include/
│   │   ├── bitmap.h          位图读写（24 位 BMP）
│   │   └── geometry.h        Vector / Ray
│   └── src/
│       ├── bitmap.cpp
│       └── geometry.cpp
│
├── docs/
│   └── sphere_preview.png    文档配图
│
├── section_2/                ★ 本节
│   ├── CMakeLists.txt        定义本节算法库与演示程序
│   ├── README.md             本文档（中文）
│   ├── README.en.md          English README
│   ├── include/
│   │   ├── object.h          抽象基类 Object + Sphere
│   │   ├── raytracer.h       Raytracer：摄像机 + 场景
│   │   └── settings.h        本节所有可调参数
│   └── src/
│       ├── object.cpp
│       ├── raytracer.cpp
│       ├── settings.cpp
│       └── main.cpp          演示入口
│
└── build/                    构建产物（不进版本控制，可随时删除重建）
    ├── bin/section_2_demo.exe
    ├── bin/sphere.bmp        程序运行后生成
    ├── lib/book_common.lib
    └── lib/section_2_raytracing.lib
```

**依赖关系是严格单向的一条链**，没有任何循环包含，也没有跨越「公共 / 章节」两层的反向依赖：

```
common/include/bitmap.h ──→ common/include/geometry.h
                                      │
                                      ↓
                          section_2/include/object.h
                                      │
                                      ↓
                         section_2/include/raytracer.h ──→ src/main.cpp
                                      ↑
                        section_2/include/settings.h
                        （只依赖标准库，不认识上面任何类型）
```

`settings.h` 只包含标准库，处在链的最上游 —— 它存数值，但**不认识** `Sphere`、`Raytracer`。
由 `main.cpp` 把数值从配置里读出来，再交给那些类型去构造。

`common/` 里的 `Bitmap` 与 `Vector`/`Ray` 是「与具体渲染算法无关」的基础设施，所以被抽出去共用；
而 `Object`、`Raytracer`、`Settings` 属于本节自己的渲染算法，留在 `section_2/` 里 ——
后续章节完全可以不采用这套设计，自己写一套。

---

## 几何设定

世界坐标是**右手系**：`x` 向右、`y` 向上、`z` 指向屏幕里侧。

```
                         摄像机 O(0, 0, 0)
                              ●
                              |\
                              | \   射线 OP
                              |  \
                              |   \
                              |    ● P  ← 位图上的像素(px, py)
                              |   /
                              |  /    投影平面：位图所在的那块矩形
                              | /     中心在 (0, 0, D)
                              |/
              ────────────────┼────────────────  z = D
                              |
                              |  D = 摄像机到平面的直线距离
                              |
                          ● 球心 C(0, 0, 800)
                            半径 R
```

- 摄像机固定在**世界原点**，朝 `+z` 看；
- 投影平面就是那张位图：一块宽高与像素数 1:1 的矩形，**中心在 `(0, 0, D)`**，
  即「平面中点的正上方 `D` 处」是摄像机；
- 像素 `(px, py)` 的世界坐标：

  ```
  P = ( px - (W - 1) / 2,   (H - 1) / 2 - py,   D )
  ```

  注意 **`y` 要翻转**：位图里 `py` 向下增大，世界坐标里 `y` 向上增大。
  用 `(W - 1) / 2` 而不是 `W / 2`，这样最左和最右像素关于中轴对称。

射线方向 `d = P - O = P`，写成 `(dx, dy, D)`。**故意不归一化** —— 因为这样 `t = 1`
恰好对应投影平面上的那个像素，判断「交点在摄像机前方」只需看 `t` 的正负，省掉一次开方。

---

## 代码结构与职责划分

五个模块，每个只做一件事：

| 模块 | 职责 | 不负责 |
|---|---|---|
| `Bitmap` | 一张内存中的 24 位 BMP 位图：填色、改像素、写盘 | 图上画的是什么 |
| `Vector` / `Ray` | 三维向量运算、射线参数方程 | 任何场景概念 |
| `Object` / `Sphere` | 一个物体：自己长什么样、射线和它交在哪里 | 怎么渲染、怎么涂像素 |
| `Raytracer` | 摄像机 + 场景：为每个像素发射线、挑最近的物体、涂色 | 具体形状怎么求交 |
| `Settings` | 存放所有可调参数 | 不认识上面任何类型 |

两个关键的设计边界：

**1. `Object` 不认识 `Bitmap`。** 基类只声明两件事：

```cpp
class Object {
public:
    virtual ~Object() = default;
    virtual std::optional<double> intersect(const Ray& ray) const = 0;  // 交在哪里
    virtual const uint8_t (&color() const)[3] = 0;                      // 什么颜色
protected:
    explicit Object(const uint8_t (&rgb)[3]);
    static double checkPositive(double value, const char* what);        // 各形状共用的尺寸校验
    uint8_t color_[3];
};
```

刻意**没有**「把颜色画进位图」这类接口：一来形状不该认识位图，二来以后要加光照时，
颜色得靠交点位置和法线算出来，让形状自己涂像素会立刻卡死。

**2. 物体用 `unique_ptr<Object>` 持有，不是 `vector<Object>`。**
多态对象按值放进容器会发生**对象切片** —— 派生类那部分被砍掉，只剩基类，虚调用全部失效。
所以形状必须堆分配、通过指针访问。选 `unique_ptr` 而非 `shared_ptr`：场景独占物体，
所有权唯一、无引用计数开销。代价是 `Raytracer` 因此不可拷贝（已显式 `= delete`），只可移动。

---

## 怎么改参数

**所有可以调的东西都集中在 `section_2/src/settings.cpp` 的 `makeDefault()` 里** ——
这是全项目唯一写死场景数值的地方。想换效果只改这一个函数：

```cpp
Settings Settings::makeDefault()
{
    const uint8_t black[3]  = {0, 0, 0};       // 背景
    const uint8_t orange[3] = {255, 80, 0};    // 球的颜色
    const uint8_t green[3]  = {0, 200, 60};

    // ---------- 画面与摄像机 ----------
    ImageSettings image(
        640,        // 宽（像素）
        480,        // 高（像素）
        500.0,      // 摄像机到投影平面的距离 D
        black);     // 背景色

    // ---------- 场景 ----------
    Settings s(image, "sphere.bmp");

    // 球：球心坐标、半径、颜色。z 要大于 D，才在投影平面前方（能被看到）。
    s.addSphere(SphereSettings(0.0, 0.0, 800.0, 150.0, orange));

    // 想再加一个球，去掉下面这行的注释即可：
    // s.addSphere(SphereSettings(120.0, 0.0, 900.0, 100.0, green));

    return s;
}
```

球用 `std::vector` 存，**想放几个放几个**。加第二个球后绿色小球会遮住橙色大球的一部分 ——
渲染时按 `t` 取最近的命中物体，所以**添加顺序不影响结果**。

### 关于「体积」

`SphereSettings` 只存**半径**，不存体积。光线投射的正经计算只用到半径，体积是半径的派生量：

```cpp
double volume() const;   // 按 4/3 * pi * R^3 现算，只用于展示
```

两者都存的话，万一改了一个忘了另一个，数据就自相矛盾了。

### 参数校验分两层

1. **构造时**：`SphereSettings` 校验半径、`ImageSettings` 校验宽高与距离 `D`。
   对象只要存在就一定合法，后面读出来用的时候不必再检查。
2. **渲染前**：`Settings::validate()` 补上「整体」层面的检查（场景不能是空的、输出路径不能为空）。

这样错误在动手渲染**之前**就抛出来，而不是渲染到一半才失败。负数、`0`、`NaN` 全部会被挡住：

```
错误：Settings: 球半径 必须为正数
错误：Settings: 宽 必须为正整数
错误：Settings: 场景里一个物体都没有，渲染出来会是一整张背景色
```

---

## 怎么加新形状

**不需要修改任何现有文件**（除了在 `main.cpp` 里加两行）。新建 `section_2/include/box.h` 与 `section_2/src/box.cpp`：

```cpp
#include "object.h"     // 只需要这一个项目头文件

class Box : public Object {
public:
    Box(const Vector& mn, const Vector& mx, const uint8_t (&rgb)[3])
        : Object(rgb), minX_(mn.x()), maxX_(mx.x()), /* ... */ {}

    std::optional<double> intersect(const Ray& ray) const override {
        // slab 方法：把射线与三对平行平面的相交区间取交集
        // ...
    }

    const uint8_t (&color() const)[3] override { return color_; }

private:
    double minX_, maxX_, minY_, maxY_, minZ_, maxZ_;
};
```

然后在 `main.cpp` 里加进去，并把 `box.cpp` 加进 `section_2/CMakeLists.txt` 里 `section_2_raytracing` 库的源文件列表：

```cpp
tracer.add(std::make_unique<Box>(Vector(-60, -60, 700),
                                Vector( 60,  60, 900), blue));
```

`raytracer.h` / `raytracer.cpp` / `bitmap.*` / `geometry.*` 一个字都不用动。
渲染循环里只有**一个**多态调用点：

```cpp
for (const std::unique_ptr<Object>& object : objects_) {
    const std::optional<double> t = object->intersect(ray);   // ← 唯一的类型分发
    if (!t.has_value()) { continue; }
    if (nearest == nullptr || *t < nearestT) { nearest = object.get(); nearestT = *t; }
}
```

这就是「对扩展开放、对修改关闭」。此路径已用一个临时的 `Box` 实现实测通过：球与长方体
共存、互相遮挡、顺序无关、三种形状同时渲染，全部正确。

---

## 数学原理

### 射线与球面求交

射线 `P(t) = O + t·d` 与球心 `C`、半径 `R` 的球面相交，等价于求 `t` 使 `|O + t·d - C|² = R²`。
把 `O - C` 记作 `oc`，展开成关于 `t` 的二次方程：

```
a = d·d
b = 2 · oc·d
c = oc·oc - R²
delta = b² - 4ac
```

因为方向向量 `d` 不是零向量，`a > 0` 恒成立，不会退化成一次方程。

**判别式的几何含义**（这就是「`delta >= 0` 即命中」的来历）：

```
delta = 4 · (d·d) · (R² - q²)
```

其中 `q` 是球心到这条射线**所在直线**的垂直距离。于是：

| 判别式 | 几何含义 |
|---|---|
| `delta > 0` | 直线穿过球（`q < R`），两个交点 |
| `delta = 0` | 直线与球相切（`q = R`），两根重合 |
| `delta < 0` | 直线与球不相交（`q > R`） |

**但 `delta >= 0` 只说明「直线」穿过球，不等于「能看到球」。** 如果两个根都不为正，
球整个在摄像机背后 —— 此时判别式仍为正，却什么都没挡住。所以还要取最近的正根：

```cpp
if (t0 > 0.0)      return t0;            // 正常：近交点在射线前方
if (t1 > 0.0)      return t1;            // 摄像机在球内部：近根为负，取远根
return std::nullopt;                     // 球整体在背后 → 仍涂背景色
```

### 命中区域的解析半径

命中边界 `q = R` 在投影平面 `z = D` 上是一个圆：

```
x² + y² = R² D² / (Cz² - R²)
```

所以命中圆的半径是 `R·D / √(Cz² - R²)`。默认参数下为 `95.4427` 像素，
程序会把这个值和实际命中像素数一起打印出来供对账。

> 注意：**偏离光轴的球，投影是椭圆而不是正圆**。上式的圆只描述「射线方向与球心方向的
> 夹角恰为半锥角 `θ = asin(R/|C|)`」这个锥面与平面的交线；球心在轴上时它是正圆。

---

## BMP 输出格式

自己按字节拼的 24 位 BMP，依次写出：

| 段 | 大小 | 内容 |
|---|---|---|
| 文件头 `BITMAPFILEHEADER` | 14 字节 | 类型标记 `BM`、文件总大小、像素数据偏移（恒为 54） |
| 信息头 `BITMAPINFOHEADER` | 40 字节 | 宽高、每像素 24 位、不压缩、分辨率 2835 像素/米（≈72 DPI） |
| 像素数据 | `stride × height` | 见下 |

三处容易写错、已在代码里处理的细节：

1. **像素自下而上存储** —— 位图的第一行在文件末尾。内部 `pixels_` 直接按 BMP 原生顺序
   存放，所以 `save()` 可以整块写出、零转换；代价是 `setPixel()` 里要做一次行号翻转
   `height_ - 1 - y`。
2. **每个像素是 BGR 顺序**，与惯用的 `{R, G, B}` 相反。
3. **每行补齐到 4 字节边界** —— `stride = (width × 3 + 3) / 4 × 4`。宽 640 时
   `640 × 3 = 1920` 已是 4 的倍数，无需补位。

`#pragma pack(1)` 关闭结构体对齐填充，并用 `static_assert` 在**编译期**卡住两个头的大小：

```cpp
static_assert(sizeof(BmpFileHeader) == 14, "BMP 文件头必须是 14 字节");
static_assert(sizeof(BmpInfoHeader) == 40, "BMP 信息头必须是 40 字节");
```

这样万一将来有人改动字段，编译会直接失败，而不是生成一个「能编译、却打不开」的坏文件。

---

## 验证过的正确性

渲染结果经过了多轮**独立**交叉验证，不只是「跑起来没报错」：

| 验证方式 | 结果 |
|---|---|
| 独立 Python 复刻整套光线投射（完全不同语言、独立实现） | 生成的 BMP 与 C++ 输出 **SHA256 完全相同** |
| 纯代数判据（像素是否落在圆锥截线内）对全部 307200 个像素逐点比对 | **零分歧** |
| 判别式恒等式 `delta = 4(d·d)(R² - q²)` 扫描 10 万条射线 | 最大相对偏差 `3.2e-12` |
| 命中面积 vs 解析值 `π(R·D/√(Cz²-R²))²` | 误差 **0.16%**（像素量化的离散误差） |
| 坐标方向：球移到世界 `+x` / `+y` | 命中区包围盒中心与轴交点理论值**精确吻合**（`px=137`、`py=62`） |
| 参数校验：半径/宽高/距离为 `0`、负数、`NaN` | 全部正确抛 `std::invalid_argument` |
| 多形状遮挡、添加顺序无关、三种形状共存 | 全部通过（用临时 `Box` 实现） |

编译始终使用 `/W4`（MSVC 最高警告级别）并保持**零警告**。项目里的 `#pragma pack` 与
`static_assert` 保证 BMP 头的字节布局在编译期就是对的。

---

## 已知限制

这些是当前实现的**有意取舍**，不是缺陷：

- **没有光照模型。** 只有纯色剪影，物体表面没有明暗和立体感。要加光照就需要在
  `Object` 里补一个 `normal()` 法线接口（当前基类刻意只留了 `intersect` 与 `color`）。
- **没有阴影、反射、折射。** 只从摄像机发一次射线，不做二次弹射。
- **求交是线性扫描。** `tracePixel()` 遍历场景里所有物体，复杂度 `O(像素数 × 物体数)`。
  物体成千上万时需要引入 BVH 之类的加速结构 —— 替换点就在 `tracePixel()` 这一个函数里。
- **不支持纹理、不抗锯齿。** 每个像素只发一条射线，边缘是硬边。
- **只写 24 位未压缩 BMP，不能读入图片。** `Bitmap` 是只出不进的。
- **球不会互相投射阴影**，也不会被裁剪 —— 纯粹是几何可见性判断。
- **`Settings` 只能在代码里改。** 没有配置文件解析；这是刻意的取舍，避免为教学项目引入
  一份解析和容错的代码。

---

## 许可

本项目为教学示例代码，可自由使用与修改。
