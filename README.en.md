# Hand-Written Graphics Rendering · Tutorial Code

Tutorial code for **writing graphics/rendering programs from scratch in pure C++17**,
organised by chapter. No third-party dependencies — even the BMP file writing is assembled
byte by byte by hand.

Code lives in per-chapter directories, while infrastructure shared across chapters lives in
`common/`, so later chapters do not have to reinvent it.

> 中文文档见 [README.md](README.md)。

---

## Chapters

| Chapter | Topic | Status |
|---|---|---|
| [section_2](section_2/README.en.md) | **Ray casting**: treat a 24-bit BMP bitmap as the projection plane, put the camera directly above the plane's centre, cast one ray per pixel and intersect it with spheres in the scene | ✅ Done |

![Section 2 render result: an orange sphere on a black background](docs/sphere_preview.png)

---

## Quick start

### Requirements

| Item | Requirement |
|---|---|
| Compiler | C++17 support (it uses `std::optional`). Verified on MSVC 19.51 (Visual Studio 2026) |
| Build tool | CMake ≥ 3.20. The copy bundled with Visual Studio is fine — nothing extra to install |
| OS | Verified on Windows 10/11; Linux/macOS should build too |

### Build and run

One command from the repository **root** (the script locates the CMake bundled with Visual
Studio by itself):

```powershell
.\build.ps1
```

It configures and builds the whole repository, then runs section 2's demo, writing the bitmap
to `build/bin/sphere.bmp`.

> **If you get "running scripts is disabled on this system"** — that is PowerShell's default
> ExecutionPolicy, not a problem with the script. The simplest fix changes nothing system-wide;
> it just bypasses the policy for this one invocation:
>
> ```powershell
> powershell -ExecutionPolicy Bypass -File .\build.ps1
> ```

Other options:

```powershell
.\build.ps1 -List               # list the executables that were built
.\build.ps1 -Target <name>      # run a specific executable (no .exe)
.\build.ps1 -Config Debug       # Debug build
.\build.ps1 -NoRun              # build only
.\build.ps1 -Clean              # wipe build/ and reconfigure from scratch
```

Without the script (building the whole repository from its root):

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\bin\section_2_demo.exe
```

> **Opening this in VS Code / Visual Studio**: the root already contains `CMakeLists.txt`, so
> both IDEs pick it up and configure it for you.

---

## Layout

```
book/                         repository root
├── CMakeLists.txt            root build config: defines the common library, mounts chapters
├── build.ps1                 one-command build script
├── .gitignore                excludes build artifacts
├── README.md                 Chinese README
├── README.en.md              this file
│
├── common/                   ★ code shared by all chapters
│   ├── CMakeLists.txt        defines the book_common static library
│   ├── include/
│   │   ├── bitmap.h          a 24-bit BMP in memory: fill, set pixels, write to disk
│   │   └── geometry.h        3D Vector and Ray
│   └── src/
│       ├── bitmap.cpp
│       └── geometry.cpp
│
├── docs/                     documentation assets
│   └── sphere_preview.png
│
├── section_2/                chapter 2: ray casting
│   ├── CMakeLists.txt
│   ├── README.md             detailed chapter docs (Chinese)
│   ├── README.en.md          detailed chapter docs (English)
│   ├── include/              object.h / raytracer.h / settings.h
│   └── src/                  object.cpp / raytracer.cpp / settings.cpp / main.cpp
│
└── build/                    build output (not tracked, safe to delete)
    ├── bin/section_2_demo.exe
    ├── bin/sphere.bmp        produced by running the program
    ├── lib/book_common.lib
    └── lib/section_2_raytracing.lib
```

---

## Adding a new chapter

Taking `section_3` as an example, three steps:

1. **Create the directories**: `section_3/include/`, `section_3/src/`
2. **Write `section_3/CMakeLists.txt`**, modelled on `section_2/CMakeLists.txt`: define the
   chapter's own library and executable, then
   `target_link_libraries(<target> PUBLIC book_common)` to inherit the common library's include
   paths and compile options.
3. **Append one line** to the root `CMakeLists.txt`:

   ```cmake
   add_subdirectory(section_3)
   ```

The root `CMakeLists.txt` deliberately defines **no** `add_executable` — executables belong to
their chapters, so adding a chapter costs exactly one line there.

### Chapters can be built standalone

Every chapter's `CMakeLists.txt` also works as a standalone project, bypassing the root:

```powershell
cmake -S section_2 -B build_section_2
cmake --build build_section_2 --config Release
```

This works because `section_2/CMakeLists.txt` contains an `if(NOT TARGET book_common)` guard —
when built standalone it pulls in `common/` itself, and when built from the root it does not
redefine anything. Both paths produce a **byte-identical** executable (output hashes verified).

---

## About the `common/` library

It holds only infrastructure **independent of any rendering algorithm**:

- **`Bitmap`** — a 24-bit BMP in memory. Everything about the format (file headers, BGR pixel
  order, 4-byte row alignment, bottom-up storage) is encapsulated inside the class.
- **`Vector` / `Ray`** — 3D vectors and the parametric ray equation, general geometry primitives.

The rule of thumb is simple: **if a thing would still be useful under a different rendering
algorithm, it belongs in `common/`.** Conversely, section 2's `Object` base class, `Raytracer`
and `Settings` embody that chapter's own design choices (for example, a base class that only
intersects and does not paint), so they stay in the chapter — later chapters are free to ignore
that design entirely.

The dependency direction is strictly one-way, with no circular includes:

```
common/  ──→  section_N/  ──→  main.cpp
```

`common/` never includes a chapter header.

---

## Encoding and build notes

- **The sources are UTF-8 without BOM, while a Simplified-Chinese Windows defaults to code page
  936 (GBK)**, so `common/CMakeLists.txt` passes `/utf-8` to MSVC. That flag is marked `PUBLIC`,
  so every chapter linking `book_common` inherits it without repeating it. **Remove it and the
  Chinese comments become mojibake.**
- **`build.ps1` is deliberately ASCII-only** (English messages and comments). Windows PowerShell
  5.1 decodes a `.ps1` file using the system ANSI code page unless it starts with a UTF-8 BOM, so
  Chinese text inside the script would be garbled and break parsing. The `.cpp` files can be fixed
  with a compiler flag; `.ps1` files cannot.
- Builds always run at the highest warning level (MSVC `/W4`, GCC/Clang
  `-Wall -Wextra -pedantic`) and stay **warning-free**.

---

## License

Teaching sample code — free to use and modify.
