# Ray-Cast Bitmap Renderer

A **ray caster** that treats a **24-bit BMP bitmap** as the projection plane and renders spheres.
Pure C++17 standard library — no third-party dependencies. Even the BMP file writing is done
byte by byte by hand.

![Render result: an orange sphere centred on a black background](docs/sphere_preview.png)

The image above is the program's actual output: 640×480, sphere centre `(0, 0, 800)`,
radius `150`, colour `RGB(255, 80, 0)`, camera at the world origin, distance to the
projection plane `D = 500`.

> 中文文档见 [README.md](README.md)。

---

## Contents

- [What it does](#what-it-does)
- [Quick start](#quick-start)
- [Project layout](#project-layout)
- [Geometry](#geometry)
- [Code structure and responsibilities](#code-structure-and-responsibilities)
- [Changing parameters](#changing-parameters)
- [Adding a new shape](#adding-a-new-shape)
- [The maths](#the-maths)
- [BMP output format](#bmp-output-format)
- [Verified correctness](#verified-correctness)
- [Known limitations](#known-limitations)

---

## What it does

For **every pixel** of the bitmap:

1. Convert the pixel coordinate into a world-space point `P` on the projection plane.
2. Cast a ray from the camera (the world origin `O`) towards `P`, direction `d = P - O`.
3. Ask each object in the scene "where do you intersect this ray?".
4. Take the **nearest** hit and paint that object's colour into the pixel; if nothing is
   hit, paint the background colour.

There is no lighting model whatsoever — the discriminant only decides *hit* or *miss*,
so the result is a **flat silhouette**. The `delta >= 0` rule comes from
[the maths](#the-maths) below.

---

## Quick start

### Requirements

| Item | Requirement |
|---|---|
| Compiler | C++17 support (it uses `std::optional`). Verified on MSVC 19.51 (Visual Studio 2026) |
| Build tool | CMake ≥ 3.20. The copy bundled with Visual Studio is fine — nothing extra to install |
| OS | Verified on Windows 10/11; Linux/macOS should build too (CMakeLists handles MSVC and GCC/Clang flags separately) |

> **Opening this project in VS Code or Visual Studio**: the root already contains
> `CMakeLists.txt`, so both IDEs pick it up and configure it for you.

### Build and run

One command (the script locates the CMake bundled with Visual Studio by itself):

```powershell
.\build.ps1
```

Expected output:

```
已生成位图：sphere.bmp  640 x 480
摄像机 O(0,0,0)，投影平面 z = 500
背景色 RGB(0, 0, 0)
场景物体数：1
  球 #0  球心 (0, 0, 800)  R = 150  体积 = 1.41372e+07  颜色 RGB(255, 80, 0)
        命中圆半径约 95.4427 像素
```

The bitmap is written to `build/bin/sphere.bmp`.
(Program output is Chinese; the numbers are what matter.)

<details>
<summary><b>If you get "running scripts is disabled on this system"</b></summary>

That is PowerShell's default ExecutionPolicy, not a problem with the script. Pick one:

```powershell
# 1. Bypass for this one invocation only (simplest, changes nothing)
powershell -ExecutionPolicy Bypass -File .\build.ps1

# 2. Allow signed scripts for the current user (one-off, permanent)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# 3. Skip the script and run CMake yourself
```

</details>

### Without the script

**Option 1: build the whole repository from its root** (recommended — predictable paths)

```powershell
cd <repository root>
cmake -S . -B build
cmake --build build --config Release
.\build\bin\section_2_demo.exe
```

**Option 2: build just this section**, treating it as a standalone project

```powershell
cmake -S section_2 -B build_section_2
cmake --build build_section_2 --config Release
.\build_section_2\bin\section_2_demo.exe
```

On that path `section_2/CMakeLists.txt` pulls in `common/` by itself and artefacts land in
`build_section_2/`. Both paths produce **byte-identical** executables — the output hashes match
exactly.

### Other options

```powershell
.\build.ps1 -Config Debug      # Debug build
.\build.ps1 -NoRun             # build only
.\build.ps1 -Clean             # wipe build/ and reconfigure from scratch
.\build.ps1 -List              # list the executables that were built
.\build.ps1 -Target <name>     # run a specific executable (no .exe)
```

---

## Project layout

This section is one chapter of a repository. **Code shared across chapters lives in `common/`
at the repository root**, so later chapters can reuse it:

```
book/                         repository root
├── CMakeLists.txt            root build config: defines the common library, mounts chapters
├── build.ps1                 one-command build script
├── .gitignore                excludes build artifacts
│
├── common/                   ★ code shared by all chapters
│   ├── CMakeLists.txt        defines the book_common static library
│   ├── include/
│   │   ├── bitmap.h          24-bit BMP storage and writing
│   │   └── geometry.h        Vector / Ray
│   └── src/
│       ├── bitmap.cpp
│       └── geometry.cpp
│
├── docs/
│   └── sphere_preview.png    documentation asset
│
├── section_2/                ★ this chapter
│   ├── CMakeLists.txt        defines this chapter's library and demo program
│   ├── README.md             Chinese README
│   ├── README.en.md          this file
│   ├── include/
│   │   ├── object.h          abstract base class Object + Sphere
│   │   ├── raytracer.h       Raytracer: camera + scene
│   │   └── settings.h        every tunable parameter for this chapter
│   └── src/
│       ├── object.cpp
│       ├── raytracer.cpp
│       ├── settings.cpp
│       └── main.cpp          demo entry point
│
└── build/                    build output (not tracked, safe to delete)
    ├── bin/section_2_demo.exe
    ├── bin/sphere.bmp        produced by running the program
    ├── lib/book_common.lib
    └── lib/section_2_raytracing.lib
```

**The dependency chain is strictly one-directional**, with no circular includes and no
back-edges across the common/chapter boundary:

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
                        (standard library only; knows none of the above)
```

`settings.h` only includes standard headers and sits at the very top of the chain — it stores
values but does **not** know about `Sphere` or `Raytracer`. `main.cpp` reads the values out of
the settings and hands them to those types.

In `common/`, `Bitmap` and `Vector`/`Ray` are infrastructure **independent of any rendering
algorithm**, which is why they were extracted for reuse. `Object`, `Raytracer` and `Settings`
belong to this chapter's own rendering design and stay in `section_2/` — later chapters are
free to ignore that design entirely and write their own.

---

## Geometry

World space is **right-handed**: `x` right, `y` up, `z` into the screen.

```
                          camera O(0, 0, 0)
                              ●
                              |\
                              | \   ray OP
                              |  \
                              |   \
                              |    ● P  ← pixel (px, py) of the bitmap
                              |   /
                              |  /    projection plane: the rectangle the bitmap lives on,
                              | /     centred at (0, 0, D)
                              |/
              ────────────────┼────────────────  z = D
                              |
                              |  D = distance from camera to plane
                              |
                          ● sphere centre C(0, 0, 800)
                            radius R
```

- The camera sits at the **world origin**, looking along `+z`.
- The projection plane *is* the bitmap: a rectangle whose width and height match the pixel
  counts 1:1, **centred at `(0, 0, D)`** — i.e. the camera is `D` directly "above" the plane's centre.
- World coordinates of pixel `(px, py)`:

  ```
  P = ( px - (W - 1) / 2,   (H - 1) / 2 - py,   D )
  ```

  Note the **`y` flip**: `py` grows downwards in a bitmap, while world `y` grows upwards.
  `(W - 1) / 2` (not `W / 2`) keeps the leftmost and rightmost pixels symmetric about the axis.

The ray direction is `d = P - O = P`, written as `(dx, dy, D)`. It is **deliberately left
unnormalised** — that way `t = 1` lands exactly on the projection plane, so testing whether a
hit is in front of the camera is just a sign check on `t`, saving a square root.

---

## Code structure and responsibilities

Five modules, each doing one thing:

| Module | Responsible for | Not responsible for |
|---|---|---|
| `Bitmap` | A 24-bit BMP in memory: fill, set pixels, write to disk | What is drawn on it |
| `Vector` / `Ray` | 3D vector maths, parametric ray | Any scene concept |
| `Object` / `Sphere` | An object: what it looks like, where a ray hits it | How to render or paint pixels |
| `Raytracer` | Camera + scene: cast a ray per pixel, pick the nearest object, paint it | How a given shape intersects |
| `Settings` | Holding every tunable parameter | Knowing about any of the above |

Two design boundaries matter:

**1. `Object` does not know about `Bitmap`.** The base class declares exactly two things:

```cpp
class Object {
public:
    virtual ~Object() = default;
    virtual std::optional<double> intersect(const Ray& ray) const = 0;  // where
    virtual const uint8_t (&color() const)[3] = 0;                      // what colour
protected:
    explicit Object(const uint8_t (&rgb)[3]);
    static double checkPositive(double value, const char* what);        // shared size validation
    uint8_t color_[3];
};
```

There is deliberately **no** "paint your colour into the bitmap" method: a shape should not
know about bitmaps, and once lighting is added the colour must be computed from the hit point
and normal — letting shapes paint themselves would block that immediately.

**2. Objects are held as `unique_ptr<Object>`, never `vector<Object>`.**
Storing polymorphic objects by value in a container causes **object slicing** — the derived
part is chopped off, leaving only the base, and every virtual call silently goes to the base.
Shapes must therefore live on the heap and be accessed through pointers. `unique_ptr` rather
than `shared_ptr`, because the scene exclusively owns its objects: unique ownership, no
reference-count overhead, and no risk of two scenes secretly sharing an object. The cost is
that `Raytracer` becomes non-copyable (explicitly `= delete`d) and movable only.

---

## Changing parameters

**Everything tunable lives in `makeDefault()` in `section_2/src/settings.cpp`** — the only place in the
project where scene values are hard-coded. Change that one function to change the picture:

```cpp
Settings Settings::makeDefault()
{
    const uint8_t black[3]  = {0, 0, 0};       // background
    const uint8_t orange[3] = {255, 80, 0};    // sphere colour
    const uint8_t green[3]  = {0, 200, 60};

    // ---------- image and camera ----------
    ImageSettings image(
        640,        // width  (pixels)
        480,        // height (pixels)
        500.0,      // distance D from camera to projection plane
        black);     // background colour

    // ---------- scene ----------
    Settings s(image, "sphere.bmp");

    // Sphere: centre, radius, colour. z must exceed D so it is in front of the plane.
    s.addSphere(SphereSettings(0.0, 0.0, 800.0, 150.0, orange));

    // Uncomment to add a second sphere:
    // s.addSphere(SphereSettings(120.0, 0.0, 900.0, 100.0, green));

    return s;
}
```

Spheres live in a `std::vector`, so **you can add as many as you like**. With a second sphere
the small green one occludes part of the large orange one — hits are resolved by taking the
smallest `t`, so **the order in which you add them does not matter**.

### About "volume"

`SphereSettings` stores only the **radius**, never the volume. Ray casting only ever needs the
radius; volume is derived from it:

```cpp
double volume() const;   // computes 4/3 * pi * R^3 on demand, for display only
```

Storing both invites the two to disagree after someone edits just one of them.

### Validation happens in two layers

1. **At construction**: `SphereSettings` validates the radius, `ImageSettings` validates the
   dimensions and distance `D`. If an object exists, it is valid — no re-checking needed later.
2. **Before rendering**: `Settings::validate()` covers whole-configuration sanity (the scene
   must not be empty, the output path must not be empty).

So errors surface *before* rendering starts rather than halfway through. Negative numbers,
zero and `NaN` are all rejected:

```
错误：Settings: 球半径 必须为正数
错误：Settings: 宽 必须为正整数
错误：Settings: 场景里一个物体都没有，渲染出来会是一整张背景色
```

---

## Adding a new shape

**No existing file needs to change.** Create `section_2/include/box.h` and `section_2/src/box.cpp`:

```cpp
#include "object.h"     // the only project header you need

class Box : public Object {
public:
    Box(const Vector& mn, const Vector& mx, const uint8_t (&rgb)[3])
        : Object(rgb), minX_(mn.x()), maxX_(mx.x()), /* ... */ {}

    std::optional<double> intersect(const Ray& ray) const override {
        // slab method: intersect the ray's interval with three pairs of parallel planes
        // ...
    }

    const uint8_t (&color() const)[3] override { return color_; }

private:
    double minX_, maxX_, minY_, maxY_, minZ_, maxZ_;
};
```

Then use it in `main.cpp` (and add `box.cpp` to the source list of the `section_2_raytracing`
library in `section_2/CMakeLists.txt`):

```cpp
tracer.add(std::make_unique<Box>(Vector(-60, -60, 700),
                                Vector( 60,  60, 900), blue));
```

`raytracer.h`, `raytracer.cpp`, `bitmap.*` and `geometry.*` are untouched. The render loop has
exactly **one** polymorphic call site:

```cpp
for (const std::unique_ptr<Object>& object : objects_) {
    const std::optional<double> t = object->intersect(ray);   // ← the only dispatch point
    if (!t.has_value()) { continue; }
    if (nearest == nullptr || *t < nearestT) { nearest = object.get(); nearestT = *t; }
}
```

That is "open for extension, closed for modification". This path has been exercised with a
throwaway `Box` implementation: spheres and boxes coexisting, occluding each other,
order-independent, three shapes rendered at once — all correct.

---

## The maths

### Ray–sphere intersection

The ray `P(t) = O + t·d` meets a sphere of centre `C` and radius `R` where
`|O + t·d - C|² = R²`. Writing `oc` for `O - C`, this expands into a quadratic in `t`:

```
a = d·d
b = 2 · oc·d
c = oc·oc - R²
delta = b² - 4ac
```

Since the direction `d` is not the zero vector, `a > 0` always holds, so it never degenerates
into a linear equation.

**The geometric meaning of the discriminant** (this is where the `delta >= 0` rule comes from):

```
delta = 4 · (d·d) · (R² - q²)
```

where `q` is the perpendicular distance from the sphere centre to the *line* containing the ray:

| Discriminant | Meaning |
|---|---|
| `delta > 0` | the line passes through the sphere (`q < R`), two intersections |
| `delta = 0` | the line is tangent (`q = R`), the two roots coincide |
| `delta < 0` | the line misses the sphere (`q > R`) |

**But `delta >= 0` only means the *line* crosses the sphere — not that the sphere is visible.**
If both roots are non-positive the sphere is entirely behind the camera: the discriminant is
still positive, yet nothing is in the way. So the nearest positive root must be taken:

```cpp
if (t0 > 0.0)      return t0;            // normal case: near hit is in front
if (t1 > 0.0)      return t1;            // camera inside the sphere: near root negative
return std::nullopt;                     // sphere entirely behind → still background
```

### Closed-form radius of the hit region

The boundary `q = R` traces a circle on the projection plane `z = D`:

```
x² + y² = R² D² / (Cz² - R²)
```

so the hit region's radius is `R·D / √(Cz² - R²)`. With the defaults that is `95.4427` pixels,
and the program prints this alongside the actual hit count so you can reconcile the two.

> Note: **an off-axis sphere projects to an ellipse, not a circle.** The circle above describes
> the intersection of the cone `θ = asin(R/|C|)` with the plane; it is a true circle only when
> the sphere centre lies on the view axis.

---

## BMP output format

A hand-assembled 24-bit BMP, written in order:

| Section | Size | Contents |
|---|---|---|
| File header `BITMAPFILEHEADER` | 14 bytes | magic `BM`, total file size, pixel-data offset (always 54) |
| Info header `BITMAPINFOHEADER` | 40 bytes | dimensions, 24 bits per pixel, uncompressed, 2835 px/m (≈72 DPI) |
| Pixel data | `stride × height` | see below |

Three details that are easy to get wrong, all handled in the code:

1. **Pixels are stored bottom-up** — the bitmap's first row lives at the end of the file.
   `pixels_` is kept in native BMP order internally, so `save()` writes the whole block with
   zero conversion; the price is one row flip (`height_ - 1 - y`) inside `setPixel()`.
2. **Each pixel is BGR**, the reverse of the usual `{R, G, B}`.
3. **Every row is padded to a 4-byte boundary** — `stride = (width × 3 + 3) / 4 × 4`. At width
   640, `640 × 3 = 1920` is already a multiple of 4, so no padding is needed.

`#pragma pack(1)` disables struct padding, and `static_assert` pins the two header sizes at
**compile time**:

```cpp
static_assert(sizeof(BmpFileHeader) == 14, "BMP file header must be 14 bytes");
static_assert(sizeof(BmpInfoHeader) == 40, "BMP info header must be 40 bytes");
```

So if someone edits a field later, the build fails outright instead of producing a
"compiles fine but won't open" file.

---

## Verified correctness

The render output has been cross-checked against several **independent** sources — not merely
"it runs without crashing":

| Check | Result |
|---|---|
| An independent Python reimplementation of the whole ray caster | BMP **SHA256-identical** to the C++ output |
| Pure algebraic criterion (is the pixel inside the conic section?) over all 307,200 pixels | **zero disagreements** |
| Discriminant identity `delta = 4(d·d)(R² - q²)` over 100,000 rays | max relative deviation `3.2e-12` |
| Hit area vs analytic `π(R·D/√(Cz²-R²))²` | **0.16%** error (pixel quantisation) |
| Axis directions: sphere moved to world `+x` / `+y` | hit bounding-box centre matches theory exactly (`px=137`, `py=62`) |
| Validation: radius/dimensions/distance at `0`, negative, `NaN` | all correctly throw `std::invalid_argument` |
| Multi-shape occlusion, order independence, three shapes at once | all pass (with a throwaway `Box`) |

The build always uses `/W4` (MSVC's highest warning level) and stays **warning-free**. The
`#pragma pack` plus `static_assert` pair guarantees the BMP header byte layout at compile time.

---

## Known limitations

These are deliberate trade-offs, not defects:

- **No lighting model.** Flat silhouettes only — no shading, no sense of depth on the surface.
  Lighting would need a `normal()` method on `Object` (the base class intentionally exposes only
  `intersect` and `color` today).
- **No shadows, reflections or refractions.** A single ray is cast from the camera; there is no
  secondary bouncing.
- **Intersection is a linear scan.** `tracePixel()` walks every object in the scene, so the cost
  is `O(pixels × objects)`. With thousands of objects you would want an acceleration structure
  such as a BVH — the single place to change is `tracePixel()`.
- **No textures, no anti-aliasing.** One ray per pixel, so edges are hard.
- **Write-only 24-bit uncompressed BMP; it cannot read images.** `Bitmap` is output-only.
- **Spheres do not cast shadows on each other** and are not clipped — visibility is a purely
  geometric test.
- **`Settings` is code-only.** There is no config-file parsing; that is a deliberate choice to
  avoid dragging a parser and its error handling into a teaching project.

---

## License

Teaching sample code — free to use and modify.
