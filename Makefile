ifeq ($(OS),Windows_NT)
    detected_OS := Windows
else
    detected_OS := $(shell uname -s)
endif

CXX       = g++
BUILD_DIR = build
LOG_DIR	  = logs
CXXFLAGS  = -fPIE -pie -std=c++11 -I. -fopenmp -O3 -Wall -Wextra -flto -DLINK_SHADER
OMP_LIB   = -lgomp
EXE_EXT   =
RM_CMD    = rm -rf
NVCC_FLAGS = -O3 -I.

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
        LDFLAGS_CUDA += -L/usr/local/cuda/lib64
        CXXFLAGS += -I/usr/local/cuda/include
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
    BREW_PREFIX := $(shell brew --prefix libomp)
    CXXFLAGS    = -std=c++11 -I. -Xpreprocessor -fopenmp -I$(BREW_PREFIX)/include -O3 -Wall -Wextra -flto -DLINK_SHADER
    OMP_LIB     = -L$(BREW_PREFIX)/lib -lomp
endif


PKGS     = sdl2 libavcodec libavformat libavutil libswscale
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  = $(shell pkg-config --libs $(PKGS)) $(LDFLAGS_EXTRA) $(LDFLAGS_CUDA)


MAIN_OBJ = main.o
EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_BINS := $(patsubst examples/%.cpp, $(BUILD_DIR)/%$(EXE_EXT), $(EXAMPLE_SRCS))



all: print_status $(BUILD_DIR)/eshi$(EXE_EXT) examples

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



main.o: main.cpp glsl_core.h encoder.h renderer_cpu.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

shader.o: shader.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) -c shader.cpp -o shader.o


renderer_gpu.o: renderer_gpu.cu glsl_core.h renderer_gpu.h
	nvcc $(NVCC_FLAGS) -DSHADER_PATH='"shader.cpp"' -c renderer_gpu.cu -o renderer_gpu.o




$(BUILD_DIR)/%.o: examples/%.cpp glsl_core.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(BUILD_DIR)/%_gpu.o: renderer_gpu.cu examples/%.cpp glsl_core.h | $(BUILD_DIR)
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

.PHONY: all clean examples print_status
