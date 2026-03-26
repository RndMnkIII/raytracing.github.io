# Porting Ray Tracing in One Weekend to VexRiscV / openfpgaOS

## Overview

This document analyses the compatibility between the **Ray Tracing in One Weekend**
C++ examples and the **openfpgaOS SDK** C++ support layer, targeting a
**VexRiscV RV32IMF** soft-CPU running inside an FPGA (Analogue Pocket).

The ported code lives in `src/InOneWeekend_vexriscv/`.  It compiles with the
openfpgaOS SDK (`sdk.mk`) for the embedded target, and also with any standard
C++ compiler using `make app_pc` for host-side correctness testing.

---

## Target Hardware

| Property         | Value                                    |
|------------------|------------------------------------------|
| CPU              | VexRiscV                                 |
| ISA              | `rv32imafc` (base + M + A + F + C)       |
| ABI              | `ilp32f` (32-bit int/pointer, HW float)  |
| FP hardware      | **32-bit single-precision only**         |
| FP emulation     | `double` via libgcc soft-float           |
| Toolchain        | `riscv64-unknown-elf-g++` (or `riscv64-elf-`) |
| Compile flags    | `-march=rv32imafc -mabi=ilp32f`          |
| C++ runtime      | Freestanding; provided by `of_cxxabi.cpp` |
| libc             | Minimal jump-table libc via `of_libc.h`  |
| libstdc++        | **Not available**                        |

---

## Conflict Analysis

### CONFLICT 1 — `double` used throughout; only 32-bit FP hardware available

**Severity:** Critical (performance)

**Description:**  
The raytracing code uses `double` for all floating-point values.  On
`rv32imafc`, only single-precision (`float`) is hardware-accelerated.  Every
`double` operation uses a libgcc software emulation routine (typically 5–20×
slower than a native FP instruction).  For a ray tracer this is catastrophic —
each ray bounce involves dozens of FP operations.

**Affected files:** All source files (`vec3.h`, `ray.h`, `camera.h`,
`interval.h`, `hittable.h`, `sphere.h`, `material.h`, `color.h`, `rtweekend.h`)

**Solution implemented:**  
Replace every `double` type with `float`, and add `f` suffix to all
floating-point literals (e.g. `1.0` → `1.0f`, `0.001` → `0.001f`).

**Special case — `1e-160` threshold in `vec3.h`:**  
```cpp
// Original (vec3.h line 128)
if (1e-160 < lensq && lensq <= 1.0)

// Problem: 1e-160 underflows to 0.0f in float
// Fix: use 1e-37f (safely above float min-normal ≈ 1.18e-38f)
if (1e-37f < lensq && lensq <= 1.0f)
```

---

### CONFLICT 2 — `<cmath>` functions not in `std::` namespace via SDK

**Severity:** High (build failure)

**Description:**  
The code uses `std::sqrt`, `std::fabs`, `std::fmin`, `std::fmax`, `std::tan`,
`std::pow` etc. from `<cmath>`.  The SDK's `math.h` provides these as C global
functions (via `of_libc.h` jump table) but does **not** put them in `std::`.
Additionally:

| Function       | SDK math.h provides? | Note                          |
|----------------|----------------------|-------------------------------|
| `sqrtf`        | ✅ float             | Hardware RV32F                |
| `sinf`/`cosf`  | ✅ float             | Hardware RV32F                |
| `tanf`         | ✅ float             | Hardware RV32F                |
| `fabsf`        | ✅ float             | Hardware RV32F                |
| `fminf`/`fmaxf`| ✅ float             | Hardware RV32F                |
| `powf`         | ✅ float             | Via jump table                |
| `logf`/`expf`  | ✅ float             | Via jump table                |
| `sqrt(double)` | ✅ double            | Soft-float via jump table     |
| `sin(double)`  | ✅ double            | Soft-float via jump table     |
| `cos(double)`  | ✅ double            | Soft-float via jump table     |
| `tan(double)`  | ❌ **missing**       | No double version in SDK      |
| `fabs(double)` | ❌ **missing**       | No double version in SDK      |
| `fmin(double)` | ❌ **missing**       | No double version in SDK      |
| `fmax(double)` | ❌ **missing**       | No double version in SDK      |
| `pow(double)`  | ❌ **missing**       | No double version in SDK      |

**Solution implemented:**  
`src/InOneWeekend_vexriscv/compat/cmath` — wraps SDK `math.h` functions
into `namespace std`, providing both float overloads (hardware) and double
fallbacks (soft-float cast through float):

```cpp
// float overload — hardware-accelerated on RV32F
inline float sqrt(float x) { return sqrtf(x); }
inline float tan (float x) { return tanf(x);  }
// ...

// double fallback — note: precision limited to ~7 significant digits
inline double sqrt(double x) { return (double)sqrtf((float)x); }
inline double tan (double x) { return (double)tanf((float)x);  }
```

---

### CONFLICT 3 — `<memory>` (`std::shared_ptr`, `std::make_shared`) not available

**Severity:** High (build failure)

**Description:**  
`std::shared_ptr` and `std::make_shared` are part of `libstdc++`, which is
not present in the freestanding SDK build.  The raytracing code uses
`shared_ptr` extensively for polymorphic `hittable` and `material` objects.

**Solution implemented:**  
`src/InOneWeekend_vexriscv/compat/memory` — minimal single-threaded
reference-counting `shared_ptr`:

- Uses `operator new`/`delete` from `of_cxxabi.cpp` (already provided)
- No exceptions (`-fno-exceptions` compatible)
- No RTTI (`-fno-rtti` compatible)
- Virtual destructors in `hittable`/`material` ensure correct cleanup
- Supports implicit upcast `shared_ptr<Derived>` → `shared_ptr<Base>`
- Two heap allocations per `make_shared<T>()` call (object + ref-count int)

---

### CONFLICT 4 — `<limits>` (`std::numeric_limits`) not available

**Severity:** High (build failure)

**Description:**  
`rtweekend.h` uses `std::numeric_limits<double>::infinity()` for the
`infinity` constant.  `<limits>` is part of `libstdc++`.

**Solution implemented:**  
`src/InOneWeekend_vexriscv/compat/limits` — minimal specialisations for
`float`, `double`, `int`, and `unsigned int`.  The `double` specialisation
returns `(double)__builtin_huge_valf()` since on RV32IMF the float range
is effectively the limit.

---

### CONFLICT 5 — `std::vector` (in `hittable_list.h`) not available

**Severity:** High (build failure)

**Description:**  
`hittable_list` stores objects as `std::vector<shared_ptr<hittable>>`.
`std::vector` requires `libstdc++` and heap-based dynamic arrays.

**Solution implemented:**  
`src/InOneWeekend_vexriscv/hittable_list.h` uses a fixed-capacity
`shared_ptr<hittable>` array:

```cpp
#ifndef HITTABLE_LIST_MAX
#define HITTABLE_LIST_MAX 512  // InOneWeekend final scene: ~490 spheres
#endif

class hittable_list : public hittable {
    shared_ptr<hittable> objects[HITTABLE_LIST_MAX];
    int count;
    // ...
};
```

Memory: 512 × 8 bytes = 4 KB (two pointers each on RV32).

---

### CONFLICT 6 — `std::clog` not defined in SDK iostream

**Severity:** Medium (build failure)

**Description:**  
`camera.h` uses `std::clog` to write progress messages.  The SDK's `iostream`
only defines `std::cout`, `std::cerr`, and `std::cin`.

**Solution implemented:**  
Replace `std::clog` with `std::cerr` in the ported `camera.h`.

---

### CONFLICT 7 — `std::pow` with integer exponent

**Severity:** Low (build failure/ambiguity)

**Description:**  
`material.h` calls `std::pow((1 - cosine), 5)` where `5` is an `int`.  In
standard `<cmath>` there is a `std::pow(float, int)` overload.  The compat
`cmath` only defines `pow(float, float)` and `pow(float, int)` (forwarding
to `powf`).

**Solution implemented:**  
Changed literal `5` to `5.0f` in the ported `material.h`:
```cpp
return r0 + (1.0f - r0) * std::pow((1.0f - cosine), 5.0f);
```

---

### CONFLICT 8 — `-fno-exceptions` / `-fno-rtti` SDK flags

**Severity:** Low (no changes needed)

**Description:**  
The SDK compiles with `-fno-exceptions -fno-rtti`.  The raytracing code:
- Uses **virtual functions** — ✅ works without RTTI (virtual dispatch uses vtable, not typeinfo)
- Does **not** use `dynamic_cast` or `typeid` — ✅ no RTTI required
- Does **not** `throw` exceptions — ✅ no exceptions needed
- Uses `= default` destructors — ✅ fine with `-fno-exceptions`

No changes required for this conflict.

---

### CONFLICT 9 — `std::rand()` / `<cstdlib>` vs. `rand()` / `<stdlib.h>`

**Severity:** Low (name resolution only)

**Description:**  
The original `rtweekend.h` uses `std::rand()` via `#include <cstdlib>`.
The SDK provides `rand()` in the global namespace via `stdlib.h`; there is
no `<cstdlib>` mapping to `std::rand()`.

**Solution implemented:**  
Changed to `#include <stdlib.h>` and call `rand()` directly (global namespace).

---

### CONFLICT 10 — `std::flush` stream manipulator

**Severity:** None (already works)

**Description:**  
`camera.h` uses `std::flush` as a stream manipulator.  The SDK's `iostream`
defines:
```cpp
inline ostream &flush(ostream &os) { return os.flush(); }
```
and `ostream::operator<<(ostream &(*manip)(ostream &))`.  No changes needed.

---

### CONFLICT 11 — Output channel (PPM → UART / video framebuffer)

**Severity:** Medium (runtime concern)

**Description:**  
The renderer writes PPM to `stdout` (fd 1).  On the Pocket platform, fd 1
is typically a UART debug channel, not a display.  For visual output, the
rendered pixels must be written to the video framebuffer via `of_video.h`.

**Not yet implemented (future work):**  
A `write_pixel_to_framebuffer(x, y, r, g, b)` function using the SDK's
`of_video.h` API would replace `write_color()`.  The porting scaffold
(especially the color conversion in `color.h`) makes this straightforward
to add.

---

## Summary Table

| # | Issue                           | Severity | Status   |
|---|---------------------------------|----------|----------|
| 1 | `double` → slow soft-float      | Critical | ✅ Fixed  |
| 2 | `<cmath>` in `std::` namespace  | High     | ✅ Fixed  |
| 3 | `std::shared_ptr` / `<memory>`  | High     | ✅ Fixed  |
| 4 | `std::numeric_limits` / `<limits>` | High  | ✅ Fixed  |
| 5 | `std::vector` / `<vector>`      | High     | ✅ Fixed  |
| 6 | `std::clog` missing             | Medium   | ✅ Fixed  |
| 7 | `std::pow` int exponent         | Low      | ✅ Fixed  |
| 8 | `-fno-exceptions` / `-fno-rtti` | Low      | N/A (no changes needed) |
| 9 | `std::rand()` / `<cstdlib>`     | Low      | ✅ Fixed  |
| 10| `std::flush` manipulator        | None     | N/A (already works)     |
| 11| Output to display framebuffer   | Medium   | 🔲 Future work          |

---

## File Structure

```
src/InOneWeekend_vexriscv/
├── Makefile              SDK build + PC test build
├── main.cpp              Main scene (simplified for embedded target)
├── rtweekend.h           Common types & utilities — float, SDK headers
├── vec3.h                3D vector — float
├── ray.h                 Ray — float
├── color.h               Color write — float
├── interval.h            Interval — float
├── hittable.h            Hit record + base class — float
├── hittable_list.h       Fixed-capacity object list (replaces std::vector)
├── sphere.h              Sphere hittable — float
├── material.h            lambertian / metal / dielectric — float
├── camera.h              Camera / renderer — float, reduced defaults
└── compat/
    ├── new               Placement new (freestanding builds)
    ├── cmath             std:: math wrappers → SDK float ops
    ├── limits            Minimal std::numeric_limits
    └── memory            Minimal std::shared_ptr / make_shared
```

---

## Building

### Prerequisites

```bash
# Clone the openfpgaOS SDK alongside this repo
git clone https://github.com/RndMnkIII/openfgpaOS-SDK
# Ensure the RISC-V toolchain is in PATH
which riscv64-unknown-elf-gcc || which riscv64-elf-gcc
```

### Embedded build (VexRiscV app.elf)

```bash
cd src/InOneWeekend_vexriscv
export SDK_DIR=/path/to/openfgpaOS-SDK/src/sdk
make
# Produces: app.elf  (RV32IMF, ready to load on the Pocket)
```

### PC / host build (correctness testing)

```bash
cd src/InOneWeekend_vexriscv
make app_pc
./app_pc > out.ppm 2>/dev/null
# View out.ppm with any PPM viewer (e.g. GIMP, feh, ImageMagick)
```

### Full random-sphere scene

```bash
# ~490 spheres (fits in HITTABLE_LIST_MAX=512)
make app_pc CXXFLAGS=-DFULL_SCENE
./app_pc > full_scene.ppm 2>/dev/null
```

---

## Performance Notes

On a VexRiscV running at ~50 MHz, rendering 160×90 pixels with 4 samples and
8 bounces involves approximately:

| Metric                    | Estimate          |
|---------------------------|-------------------|
| Rays cast                 | 160×90×4 = 57 600 |
| FP ops per ray (typical)  | ~200              |
| Total FP ops              | ~11 million       |
| FP throughput (RV32F)     | ~50 Mflops        |
| Estimated render time     | ~1 second         |

These estimates assume good cache performance and no bus stalls.  Actual
time may be higher due to memory latency and the recursive nature of the
ray-bounce algorithm.

For comparison, the original 1200×675 / 10 samples / 20-bounce scene at
double precision would be **~1000× slower** on this target.

---

## TheNextWeek Port (`src/TheNextWeek_VexRiscV/`)

The full port of *Ray Tracing: The Next Week* is implemented in
`src/TheNextWeek_VexRiscV/`.  It follows the same methodology as the
InOneWeekend port, with the additional challenges described below.

### Additional conflicts resolved for TheNextWeek

#### CONFLICT TNW-1 — `std::vector` / `<vector>` not available

**Severity:** High (build failure)

**Description:**  
`hittable_list.h` and `bvh.h` use `std::vector<shared_ptr<hittable>>`.
Unlike the InOneWeekend port (which replaced `std::vector` with a fixed-size
array), the TheNextWeek port **preserves the dynamic vector** because `bvh_node`
requires a mutable, sortable sequence — replacing it with a fixed array would
change the program's logic and the BVH algorithm.

**Solution implemented:**  
`src/TheNextWeek_VexRiscV/compat/vector` — a minimal `std::vector<T>` that:
- Allocates raw storage with `operator new(n * sizeof(T))`
- Constructs and destroys elements via placement new / `~T()` calls
- Exposes pointer-based iterators (`T* begin() / T* end()`)
- Supports `push_back`, `operator[]`, `size()`, `clear()`, copy constructor
- Is compatible with the range-based for loops and `std::sort` used in the code

#### CONFLICT TNW-2 — `std::sort` / `<algorithm>` not available

**Severity:** High (build failure)

**Description:**  
`bvh.h` uses `std::sort` and `std::begin`/`std::end` from `<algorithm>`.
Eliminating the sort would break the BVH construction logic.

**Solution implemented:**  
`src/TheNextWeek_VexRiscV/compat/algorithm` — provides:
- `std::sort(first, last, comp)` via **insertion sort** — correct O(n²)
  alternative.  BVH construction is a build-time (not render-time) operation
  on small arrays, so O(n²) is acceptable.  Sorting is **not eliminated**.
- `std::begin(container)` / `std::end(container)` forwarding helpers.

#### CONFLICT TNW-3 — `stb_image.h` / `std::string` / filesystem not available

**Severity:** Medium (image textures not functional on embedded)

**Description:**  
`rtw_stb_image.h` uses `std::string` for path construction and `getenv`/file
I/O for loading images.  The VexRiscV target has no accessible filesystem.

**Solution implemented:**  
`src/TheNextWeek_VexRiscV/rtw_stb_image.h` uses an `#ifdef OF_PC` guard:
- **PC build:** full `stb_image` implementation, identical to the original.
- **Embedded build:** stub `rtw_image` that returns `height() == 0`.
  This activates the existing cyan fallback in `image_texture::value()` —
  the same "no image data" debug path already present in the original code.

#### CONFLICT TNW-4 — Additional `double` types in new files

**Severity:** Critical (performance)

**Files affected:** `aabb.h`, `bvh.h`, `perlin.h`, `texture.h`, `quad.h`,
`constant_medium.h`, `camera.h` (background, ray_time), `material.h`
(emitted, diffuse_light, isotropic), `sphere.h` (moving sphere, UV mapping),
`ray.h` (time), `hittable.h` (u/v coords, rotate_y, translate).

**Solution:** Same as InOneWeekend — `double` → `float` throughout, `f`
suffix on all literals.

---

### TheNextWeek File Structure

```
src/TheNextWeek_VexRiscV/
├── Makefile              SDK build + PC test build
├── main.cpp              All 9 original scenes, SCENE=N compile-time selector
├── rtweekend.h           Common types & utilities — float, SDK headers + random_int
├── vec3.h                3D vector — float
├── ray.h                 Ray — float (+ time for motion blur)
├── color.h               Color write — float
├── interval.h            Interval — float (+ expand, spanning ctor, operator+)
├── aabb.h                Axis-aligned bounding box — float (new for TheNextWeek)
├── hittable.h            Hit record + base + translate + rotate_y — float
├── hittable_list.h       std::vector-based list + bounding_box (via compat/vector)
├── bvh.h                 BVH — float, uses compat/algorithm (sort preserved)
├── perlin.h              Perlin noise — float
├── texture.h             Textures (solid, checker, image, noise) — float
├── rtw_stb_image.h       OF_PC: full stb_image; embedded: stub (cyan fallback)
├── material.h            All materials + diffuse_light + isotropic — float
├── sphere.h              Sphere (stationary + moving) + UV mapping — float
├── quad.h                Quad + box() helper — float
├── constant_medium.h     Constant medium (volumetric) — float
├── camera.h              Camera/renderer + background + emission — float
└── compat/
    ├── new               Placement new (freestanding builds)
    ├── cmath             std:: math wrappers → SDK float ops
    ├── limits            Minimal std::numeric_limits
    ├── memory            Minimal std::shared_ptr / make_shared
    ├── vector            Minimal std::vector<T> (new — TheNextWeek only)
    └── algorithm         Minimal std::sort + std::begin/end (new — TheNextWeek only)
```

### Building TheNextWeek_VexRiscV

#### Embedded build (VexRiscV app.elf)

```bash
cd src/TheNextWeek_VexRiscV
export SDK_DIR=/path/to/openfgpaOS-SDK/src/sdk
make                         # default scene: simple_light (4 objects)
make CXXFLAGS=-DSCENE=7      # cornell_box
```

#### PC / host build (correctness testing)

```bash
cd src/TheNextWeek_VexRiscV
make app_pc                  # default: simple_light (SCENE=0)
./app_pc > out.ppm 2>/dev/null

# Scene selection:
make app_pc CXXFLAGS=-DSCENE=2   # checkered spheres
make app_pc CXXFLAGS=-DSCENE=4   # Perlin spheres
make app_pc CXXFLAGS=-DSCENE=5   # quads
make app_pc CXXFLAGS=-DSCENE=6   # simple light
make app_pc CXXFLAGS=-DSCENE=7   # Cornell box
make app_pc CXXFLAGS=-DSCENE=8   # Cornell smoke
```

#### Notes on embedded scene selection

| Scene | Description              | BVH | Image texture | Recommended for embedded |
|-------|--------------------------|-----|---------------|--------------------------|
| 0/6   | simple_light             | No  | No            | ✅ Yes (default)          |
| 2     | checkered_spheres        | No  | No            | ✅ Yes                    |
| 4     | perlin_spheres           | No  | No            | ✅ Yes                    |
| 5     | quads                    | No  | No            | ✅ Yes                    |
| 7     | cornell_box              | No  | No            | ⚠️ Many objects           |
| 8     | cornell_smoke            | No  | No            | ⚠️ Many objects           |
| 1     | bouncing_spheres         | Yes | No            | ⚠️ Heap-intensive         |
| 3     | earth                    | No  | Yes (→ cyan)  | ⚠️ Image stub             |
| 9     | final_scene              | Yes | Yes (→ cyan)  | ⚠️ Very slow              |

---

## TheRestOfYourLife

The same porting methodology applies.  A port of *TheRestOfYourLife* is left
as future work.
