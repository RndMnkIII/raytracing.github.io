# RayTracing: The Next Week — VexRiscV / openfpgaOS port
#
# Target: VexRiscV RV32IMF core (rv32imafc, ABI ilp32f)
# SDK:    https://github.com/RndMnkIII/openfgpaOS-SDK

#
# ── Quick start ───────────────────────────────────────────────────────────
#
#   # Step 1: set SDK_DIR to where openfgpaOS-SDK is checked out
#   export SDK_DIR=/path/to/openfgpaOS-SDK/src/sdk
#
#   # Step 2: build for VexRiscV (default scene: simple_light)
#   make
#
#   # Step 3: build native PC version for correctness testing
#   make app_pc
#   ./app_pc > out.ppm
#   # View out.ppm with any PPM viewer (e.g. GIMP, feh, display)
#
# ── Scene selection ───────────────────────────────────────────────────────
#   Select scene at compile time with -DSCENE=N:
#     0 (default) simple_light       — 4 objects, Perlin noise + light
#     1           bouncing_spheres   — checker ground + BVH (many spheres)
#     2           checkered_spheres  — two checker spheres
#     3           earth              — image texture (cyan on embedded)
#     4           perlin_spheres     — two Perlin spheres
#     5           quads              — five coloured quads
#     6           simple_light       — same as default
#     7           cornell_box        — Cornell box
#     8           cornell_smoke      — Cornell box with smoke volumes
#     9           final_scene        — all features (very slow on embedded)
#
#   Example (PC, scene 7):
#     make app_pc CXXFLAGS=-DSCENE=7
#     ./app_pc > cornell.ppm
#
# ── Custom resolution ────────────────────────────────────────────────────
#   The camera defaults to 160×? with 4 samples and 8 bounces.
#   Edit main.cpp or add CXXFLAGS=-Dcam.image_width=N (not standard; edit directly).
# ─────────────────────────────────────────────────────────────────────────

# Path to the openfgpaOS SDK (override on command line or via $SDK_DIR)
SDK_DIR ?= ../../../../openfgpaOS-SDK/src/sdk

# C++ source files (only one translation unit; all code is in headers)
SRCS_CXX := main.cpp

# Extra flags for the compat/ headers path
CXXFLAGS += -I.

include $(SDK_DIR)/sdk.mk

# ── PC / host build ───────────────────────────────────────────────────────
# Uses the system compiler; OF_PC makes each SDK compat header include_next
# to the real system header.  No SDL2 needed — output is plain-text PPM.
PC_CC  ?= c++
PC_OUT  = app_pc

app_pc: main.cpp
	$(PC_CC) -DOF_PC -I. -I.. -O2 -Wall -Wextra -std=c++14 \
	    $(CXXFLAGS) main.cpp -lm -o $(PC_OUT)

.PHONY: app_pc clean-pc

clean-pc:
	rm -f $(PC_OUT)

sdk-clean: clean-pc
