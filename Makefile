ifeq ($(OS),Windows_NT)
    detected_OS := Windows
else
    detected_OS := $(shell uname -s)
endif

CXX       = g++
BUILD_DIR = build
CXXFLAGS  = -std=c++11 -I. -fopenmp -O3 -Wall -Wextra -flto -DLINK_SHADER
OMP_LIB   = -lgomp
EXE_EXT   = 
RM_CMD    = rm -rf


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

ifeq ($(detected_OS),Linux)
endif

PKGS     = libavcodec libavformat libavutil libswscale
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  = $(shell pkg-config --libs $(PKGS)) $(LDFLAGS_EXTRA)

MAIN_OBJ = main.o
EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_BINS := $(patsubst examples/%.cpp, $(BUILD_DIR)/%$(EXE_EXT), $(EXAMPLE_SRCS))

all: $(BUILD_DIR)/eshi$(EXE_EXT) examples
	@echo "Cleaning up object files..."
	@$(RM_CMD) *.o

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/eshi$(EXE_EXT): $(MAIN_OBJ) shader.o | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) shader.o -o $@ $(LDFLAGS) $(OMP_LIB)

examples: $(EXAMPLE_BINS)


main.o: main.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

shader.o: shader.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) -c shader.cpp -o shader.o

$(BUILD_DIR)/%$(EXE_EXT): examples/%.cpp $(MAIN_OBJ) glsl_core.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@.o
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $@.o -o $@ $(LDFLAGS) $(OMP_LIB)
	@$(RM_CMD) $@.o

clean:
	$(RM_CMD) $(BUILD_DIR) *.o *.mp4

.PHONY: all clean examples
