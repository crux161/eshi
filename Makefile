CUDA_PATH_LINUX ?= /usr/local/cuda
SUMI_REPO ?= https://github.com/crux161/libsumi.git
SUMI_VENDOR_DIR ?= libsumi
SUMI_REF ?=
PKG_CONFIG ?= pkg-config

# Zig's C++ driver is required here. `zig cc` can infer C++ while compiling a
# .cpp file, but it does not add the C++ runtime when it is used as the linker.
ifneq ($(filter default undefined,$(origin CXX)),)
CXX := zig c++
endif

BUILD_DIR ?= build
OBJ_DIR := $(BUILD_DIR)/obj
LOG_DIR ?= logs
USE_LTO ?= auto

empty :=
space := $(empty) $(empty)

ifeq ($(origin SUMI_PATH), undefined)
ifneq ($(wildcard $(SUMI_VENDOR_DIR)/include/sumi/sumi.h),)
SUMI_PATH := $(SUMI_VENDOR_DIR)
else
SUMI_PATH := ../libsumi
endif
endif

SUMI_INCLUDE_DIR := $(SUMI_PATH)/include
SUMI_HEADER := $(SUMI_INCLUDE_DIR)/sumi/sumi.h

BUILD_GOALS := $(filter-out vendor clean print_config,$(MAKECMDGOALS))
ifeq ($(MAKECMDGOALS),)
BUILD_GOALS := all
endif

ifneq ($(BUILD_GOALS),)
ifeq ($(wildcard $(SUMI_HEADER)),)
$(error libsumi headers not found at $(SUMI_HEADER). Run './vendor.sh' to fetch libsumi, or pass SUMI_PATH=/path/to/libsumi)
endif
endif

ifeq ($(OS),Windows_NT)
detected_OS := Windows
else
detected_OS := $(shell uname -s)
endif

CXX_PROGRAM := $(notdir $(firstword $(CXX)))
CXX_IS_ZIG := $(if $(findstring zig,$(CXX_PROGRAM)),1,0)

# Zig 0.16's macOS C/C++ driver uses Apple's linker. It rejects -flto because
# Zig LTO requires LLD, and -fuse-ld=lld does not switch the driver to LLD.
ifeq ($(USE_LTO),auto)
ifeq ($(detected_OS)-$(CXX_IS_ZIG),Darwin-1)
LTO_ENABLED := 0
else
LTO_ENABLED := 1
endif
else
LTO_ENABLED := $(USE_LTO)
endif

ifeq ($(LTO_ENABLED),1)
LTO_FLAGS := -flto
else
LTO_FLAGS :=
endif

ifeq ($(detected_OS)-$(CXX_IS_ZIG)-$(LTO_ENABLED),Darwin-1-1)
$(error Zig C++ LTO is unavailable on macOS because Zig's LLD cannot link Mach-O. Use USE_LTO=0 (the auto default), or use CXX=clang++ USE_LTO=1 for an Apple-Clang LTO build)
endif

PROJECT_CPPFLAGS := -I. -I$(SUMI_INCLUDE_DIR) -DLINK_SHADER
PROJECT_CXXFLAGS := -std=c++11 -O3 -Wall -Wextra $(LTO_FLAGS)
PROJECT_LDFLAGS := $(LTO_FLAGS)
OMP_LIB := -lgomp
EXE_EXT :=
RM_CMD := rm -rf

NVCC_FLAGS := -O3 -I. -I$(SUMI_INCLUDE_DIR)
NVCC_PATH := $(shell command -v nvcc 2>/dev/null)
ifeq ($(NVCC_PATH),)
ifdef CUDA_PATH
NVCC_PATH := $(CUDA_PATH)/bin/nvcc
endif
endif

ifneq ($(NVCC_PATH),)
USE_GPU := 1
GPU_BACKEND := CUDA
PROJECT_CPPFLAGS += -DUSE_CUDA
LDLIBS_CUDA := -lcudart

ifeq ($(detected_OS),Linux)
LDLIBS_CUDA += -L$(CUDA_PATH_LINUX)/lib64 -L/opt/cuda/lib
PROJECT_CPPFLAGS += -I$(CUDA_PATH_LINUX)/include
endif
ifeq ($(detected_OS),Windows)
LDLIBS_CUDA += -L"$(CUDA_PATH)/lib/x64"
endif
else
USE_GPU := 0
GPU_BACKEND := none
endif

ifeq ($(detected_OS),Windows)
FFMPEG_PATH ?= C:\ProgramData\chocolatey\lib\ffmpeg-shared\tools\ffmpeg-8.0.1-full_build-shared
PROJECT_LDFLAGS += -L"$(FFMPEG_PATH)\lib"
EXE_EXT := .exe
RM_CMD := rm -f
endif

ifeq ($(detected_OS),Darwin)
HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null)
LIBOMP_PREFIX := $(shell brew --prefix libomp 2>/dev/null)
MACOS_MAJOR := $(shell sw_vers -productVersion 2>/dev/null | cut -d. -f1)
HOMEBREW_OS_PKG_CONFIG_DIRS := $(wildcard $(HOMEBREW_PREFIX)/Library/Homebrew/os/mac/pkgconfig/$(MACOS_MAJOR))
HOMEBREW_OS_PKG_CONFIG_DIRS := $(if $(HOMEBREW_OS_PKG_CONFIG_DIRS),$(HOMEBREW_OS_PKG_CONFIG_DIRS),$(wildcard $(HOMEBREW_PREFIX)/Library/Homebrew/os/mac/pkgconfig/*))
HOMEBREW_PKG_CONFIG_DIRS := $(wildcard $(foreach formula,zlib bzip2 libpng freetype harfbuzz glib gettext pcre2 graphite2 ffmpeg sdl2 sdl2_ttf,$(HOMEBREW_PREFIX)/opt/$(formula)/lib/pkgconfig) $(HOMEBREW_PREFIX)/lib/pkgconfig $(HOMEBREW_PREFIX)/share/pkgconfig) $(HOMEBREW_OS_PKG_CONFIG_DIRS)
HOMEBREW_PKG_CONFIG_PATH := $(subst $(space),:,$(strip $(HOMEBREW_PKG_CONFIG_DIRS)))
PKG_CONFIG_PATH := $(HOMEBREW_PKG_CONFIG_PATH)$(if $(PKG_CONFIG_PATH),:$(PKG_CONFIG_PATH))
export PKG_CONFIG_PATH

USE_GPU := 1
GPU_BACKEND := Metal
PROJECT_CPPFLAGS += -DUSE_METAL -I$(LIBOMP_PREFIX)/include
PROJECT_CXXFLAGS += -Xpreprocessor -fopenmp
PROJECT_LDFLAGS += -framework Metal -framework Foundation -framework QuartzCore -Wno-nullability-completeness
OMP_LIB := -L$(LIBOMP_PREFIX)/lib -lomp
else
PROJECT_CXXFLAGS += -fopenmp
endif

ifeq ($(USE_GPU),0)
STATUS_MSG := ⚠️  No GPU compiler/backend found. Building CPU-only engine.
else
STATUS_MSG := ✅ Building CPU + $(GPU_BACKEND) engine.
endif

PKGS := sdl2 SDL2_ttf libavcodec libavformat libavutil libswscale
PKG_CONFIG_CMD = PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PKG_CONFIG)

ifneq ($(BUILD_GOALS),)
PKG_CONFIG_ERRORS := $(shell $(PKG_CONFIG_CMD) --print-errors --exists $(PKGS) 2>&1 || true)
ifneq ($(strip $(PKG_CONFIG_ERRORS)),)
$(error pkg-config could not resolve required packages: $(PKGS). $(PKG_CONFIG_ERRORS). PKG_CONFIG_PATH=$(PKG_CONFIG_PATH))
endif
PROJECT_CPPFLAGS += $(shell $(PKG_CONFIG_CMD) --cflags $(PKGS))
PROJECT_LDLIBS := $(shell $(PKG_CONFIG_CMD) --libs $(PKGS)) $(LDLIBS_CUDA) $(OMP_LIB)
endif

MAIN_OBJ := $(OBJ_DIR)/main.o
SHADER_OBJ := $(OBJ_DIR)/shader.o
METAL_OBJ := $(OBJ_DIR)/renderer_metal.o
GPU_OBJ := $(OBJ_DIR)/renderer_gpu.o
EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_OBJS := $(patsubst examples/%.cpp,$(OBJ_DIR)/examples/%.o,$(EXAMPLE_SRCS))
EXAMPLE_BINS := $(patsubst examples/%.cpp,$(BUILD_DIR)/%$(EXE_EXT),$(EXAMPLE_SRCS))

ifeq ($(USE_GPU),1)
ifeq ($(detected_OS),Darwin)
PLATFORM_OBJ := $(METAL_OBJ)
else
PLATFORM_OBJ := $(GPU_OBJ)
endif
endif

all: print_status $(BUILD_DIR)/eshi$(EXE_EXT) examples

vendor:
	@SUMI_REPO="$(SUMI_REPO)" SUMI_VENDOR_DIR="$(SUMI_VENDOR_DIR)" SUMI_REF="$(SUMI_REF)" ./vendor.sh

print_status:
	@echo "$(STATUS_MSG)"
	@echo "Compiler: $(CXX) | LTO: $(if $(filter 1,$(LTO_ENABLED)),enabled,disabled) | Output: $(BUILD_DIR)"

print_config:
	@echo "CXX=$(CXX)"
	@echo "OS=$(detected_OS)"
	@echo "GPU_BACKEND=$(GPU_BACKEND)"
	@echo "USE_LTO=$(USE_LTO)"
	@echo "LTO_ENABLED=$(LTO_ENABLED)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "SUMI_PATH=$(SUMI_PATH)"

$(BUILD_DIR) $(OBJ_DIR) $(OBJ_DIR)/examples:
	@mkdir -p $@

$(BUILD_DIR)/eshi$(EXE_EXT): $(MAIN_OBJ) $(SHADER_OBJ) $(PLATFORM_OBJ) | $(BUILD_DIR)
	$(CXX) $(PROJECT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(PROJECT_LDLIBS) $(LDLIBS)

examples: $(EXAMPLE_BINS)

$(MAIN_OBJ): main.cpp encoder.h renderer_cpu.h display.h | $(OBJ_DIR)
	$(CXX) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(PROJECT_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

$(SHADER_OBJ): shader.cpp glsl_core.h | $(OBJ_DIR)
	$(CXX) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(PROJECT_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

$(GPU_OBJ): renderer_gpu.cu renderer_gpu.h | $(OBJ_DIR)
	$(NVCC_PATH) $(NVCC_FLAGS) -DSHADER_PATH='"shader.cpp"' -c $< -o $@

$(METAL_OBJ): renderer_metal.mm renderer_metal.h | $(OBJ_DIR)
	$(CXX) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(PROJECT_CXXFLAGS) $(CXXFLAGS) -fobjc-arc -c $< -o $@

$(OBJ_DIR)/examples/%.o: examples/%.cpp | $(OBJ_DIR)/examples
	$(CXX) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(PROJECT_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/examples/%_gpu.o: renderer_gpu.cu examples/%.cpp | $(OBJ_DIR)/examples
	$(NVCC_PATH) $(NVCC_FLAGS) -DSHADER_PATH='"examples/$*.cpp"' -c $< -o $@

ifeq ($(USE_GPU),1)
ifeq ($(detected_OS),Darwin)
$(BUILD_DIR)/%$(EXE_EXT): $(OBJ_DIR)/examples/%.o $(MAIN_OBJ) $(METAL_OBJ) | $(BUILD_DIR)
	$(CXX) $(PROJECT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(PROJECT_LDLIBS) $(LDLIBS)
else
$(BUILD_DIR)/%$(EXE_EXT): $(OBJ_DIR)/examples/%.o $(MAIN_OBJ) $(OBJ_DIR)/examples/%_gpu.o | $(BUILD_DIR)
	$(CXX) $(PROJECT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(PROJECT_LDLIBS) $(LDLIBS)
endif
else
$(BUILD_DIR)/%$(EXE_EXT): $(OBJ_DIR)/examples/%.o $(MAIN_OBJ) | $(BUILD_DIR)
	$(CXX) $(PROJECT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(PROJECT_LDLIBS) $(LDLIBS)
endif

clean:
	$(RM_CMD) $(BUILD_DIR) $(LOG_DIR)

.SECONDARY:
.PHONY: all clean examples print_status print_config vendor
