
CUDA_PATH_LINUX ?= /usr/local/cuda
SUMI_REPO ?= https://github.com/crux161/libsumi.git
SUMI_VENDOR_DIR ?= libsumi
SUMI_REF ?=
PKG_CONFIG ?= pkg-config
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
SUMI_CPPFLAGS := -I. -I$(SUMI_INCLUDE_DIR)

BUILD_GOALS := $(filter-out vendor clean,$(MAKECMDGOALS))
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

CXX       = g++
BUILD_DIR = build
LOG_DIR	  = logs

CXXFLAGS  = -fPIE -pie -std=c++11 $(SUMI_CPPFLAGS) -fopenmp -O3 -Wall -Wextra -flto -DLINK_SHADER
OMP_LIB   = -lgomp
EXE_EXT   =
RM_CMD    = rm -rf

NVCC_FLAGS = -O3 $(SUMI_CPPFLAGS)

NVCC_PATH := $(shell which nvcc 2>/dev/null)
ifeq ($(NVCC_PATH),)
    ifdef CUDA_PATH
        NVCC_PATH := $(CUDA_PATH)/bin/nvcc
    endif
endif

ifneq ($(NVCC_PATH),)
    USE_GPU  = 1
    CXXFLAGS += -DUSE_CUDA
    LDFLAGS_CUDA = -lcudart

    ifeq ($(detected_OS),Linux)
        LDFLAGS_CUDA += -L$(CUDA_PATH_LINUX)/lib64 -L/opt/cuda/lib
        CXXFLAGS += -I$(CUDA_PATH_LINUX)/include
    endif
    ifeq ($(detected_OS),Windows)
        LDFLAGS_CUDA += -L"$(CUDA_PATH)/lib/x64"
    endif

    STATUS_MSG = "✅ CUDA Found. Building Hybrid (CPU + GPU) Engine."
else
    USE_GPU = 0
    STATUS_MSG = "⚠️  nvcc not found. Building CPU-Only Engine."
endif

ifeq ($(detected_OS),Windows)
    FFMPEG_PATH := C:\ProgramData\chocolatey\lib\ffmpeg-shared\tools\ffmpeg-8.0.1-full_build-shared
    LDFLAGS_EXTRA = -L"$(FFMPEG_PATH)\lib"
    EXE_EXT = .exe
    RM_CMD  = rm -f
endif

ifeq ($(detected_OS),Darwin)
    CXX         = clang++
    HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    LIBOMP_PREFIX := $(shell brew --prefix libomp)
    MACOS_MAJOR := $(shell sw_vers -productVersion 2>/dev/null | cut -d. -f1)
    HOMEBREW_OS_PKG_CONFIG_DIRS := $(wildcard $(HOMEBREW_PREFIX)/Library/Homebrew/os/mac/pkgconfig/$(MACOS_MAJOR))
    HOMEBREW_OS_PKG_CONFIG_DIRS := $(if $(HOMEBREW_OS_PKG_CONFIG_DIRS),$(HOMEBREW_OS_PKG_CONFIG_DIRS),$(wildcard $(HOMEBREW_PREFIX)/Library/Homebrew/os/mac/pkgconfig/*))
    HOMEBREW_PKG_CONFIG_DIRS := $(wildcard $(foreach formula,zlib bzip2 libpng freetype harfbuzz glib gettext pcre2 graphite2 ffmpeg sdl2 sdl2_ttf,$(HOMEBREW_PREFIX)/opt/$(formula)/lib/pkgconfig) $(HOMEBREW_PREFIX)/lib/pkgconfig $(HOMEBREW_PREFIX)/share/pkgconfig) $(HOMEBREW_OS_PKG_CONFIG_DIRS)
    HOMEBREW_PKG_CONFIG_PATH := $(subst $(space),:,$(strip $(HOMEBREW_PKG_CONFIG_DIRS)))
    PKG_CONFIG_PATH := $(HOMEBREW_PKG_CONFIG_PATH)$(if $(PKG_CONFIG_PATH),:$(PKG_CONFIG_PATH))
    export PKG_CONFIG_PATH
    CXXFLAGS    = -std=c++11 $(SUMI_CPPFLAGS) -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include -O3 -Wall -Wextra -flto -DLINK_SHADER
    OMP_LIB     = -L$(LIBOMP_PREFIX)/lib -lomp
endif


PKGS     = sdl2 SDL2_ttf libavcodec libavformat libavutil libswscale
PKG_CONFIG_CMD = PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PKG_CONFIG)

ifneq ($(BUILD_GOALS),)
PKG_CONFIG_ERRORS := $(shell $(PKG_CONFIG_CMD) --print-errors --exists $(PKGS) 2>&1 || true)
ifneq ($(strip $(PKG_CONFIG_ERRORS)),)
$(error pkg-config could not resolve required packages: $(PKGS). $(PKG_CONFIG_ERRORS). PKG_CONFIG_PATH=$(PKG_CONFIG_PATH))
endif
CXXFLAGS += $(shell $(PKG_CONFIG_CMD) --cflags $(PKGS))
LDFLAGS  = $(shell $(PKG_CONFIG_CMD) --libs $(PKGS)) $(LDFLAGS_EXTRA) $(LDFLAGS_CUDA)
else
LDFLAGS  = $(LDFLAGS_EXTRA) $(LDFLAGS_CUDA)
endif


MAIN_OBJ = main.o
EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_BINS := $(patsubst examples/%.cpp, $(BUILD_DIR)/%$(EXE_EXT), $(EXAMPLE_SRCS))


all: print_status $(BUILD_DIR)/eshi$(EXE_EXT) examples

vendor:
	@SUMI_REPO="$(SUMI_REPO)" SUMI_VENDOR_DIR="$(SUMI_VENDOR_DIR)" SUMI_REF="$(SUMI_REF)" ./vendor.sh

print_status:
	@echo $(STATUS_MSG)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)


ifeq ($(USE_GPU),1)
ESHI_DEPS = $(MAIN_OBJ) shader.o renderer_gpu.o
else
ESHI_DEPS = $(MAIN_OBJ) shader.o
endif

$(BUILD_DIR)/eshi$(EXE_EXT): $(ESHI_DEPS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(ESHI_DEPS) -o $@ $(LDFLAGS) $(OMP_LIB)

examples: $(EXAMPLE_BINS)


main.o: main.cpp encoder.h renderer_cpu.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

shader.o: shader.cpp
	$(CXX) $(CXXFLAGS) -c shader.cpp -o shader.o

renderer_gpu.o: renderer_gpu.cu renderer_gpu.h
	nvcc $(NVCC_FLAGS) -DSHADER_PATH='"shader.cpp"' -c renderer_gpu.cu -o renderer_gpu.o

$(BUILD_DIR)/%.o: examples/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%_gpu.o: renderer_gpu.cu examples/%.cpp | $(BUILD_DIR)
	nvcc $(NVCC_FLAGS) -DSHADER_PATH='"examples/$*.cpp"' -c renderer_gpu.cu -o $@

ifeq ($(USE_GPU),1)
$(BUILD_DIR)/%$(EXE_EXT): $(BUILD_DIR)/%.o $(MAIN_OBJ) $(BUILD_DIR)/%_gpu.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(OMP_LIB)
	@$(RM_CMD) $<
else
$(BUILD_DIR)/%$(EXE_EXT): $(BUILD_DIR)/%.o $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(OMP_LIB)
	@$(RM_CMD) $<
endif

clean:
	$(RM_CMD) $(BUILD_DIR) *.o *.mp4
	$(RM_CMD) *.o *.mp4
	$(RM_CMD) $(LOG_DIR)

.PHONY: all clean examples print_status vendor
