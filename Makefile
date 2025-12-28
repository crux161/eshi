ifeq ($(OS),Windows_NT)
	detected_OS := Windows
else
	dettected_OS := $(shell uname -s)
endif

# Linux
ifeq ($(detected_OS),Linux)
	CXX      	= g++
	CXXFLAGS 	= "-std=c++11 -I. -I/usr/include/x86_64-linux-gnu -fopenmp -O3 -Wall -Wextra -flto -DLINK_SHADER"
	OMP_LIB  	= -lgomp
endif

# macOS
ifeq ($(detected_OS),Darwin)
	CXX		= g++
	BREW_PREFIX     := $(shell brew --prefix libomp)
	CXXFLAGS 	= -std=c++11 -I. -Xclang -fopenmp -I$(BREW_PREFIX)/include -O3 -Wall -Wextra -flto -DLINK_SHADER
	OMP_LIB 	= -L$(BREW_PREFIX)/lib -lomp
endif

# Windows
ifeq ($(detected_OS),Windows)
	O_PREFIX	:= "C:\ProgramData\chocolatey\lib\ffmpeg-shared\tools\ffmpeg-8.0.1-full_build-shared"
	EXE_EXT 	= .exe
	RM_CMD 		= rm -f
	CXX		= g++
	CXXFLAGS	= -std=c++11 -I. -fopenmp -O3 -Wall -Wextra -flto -DLINK_SHADER
	OMP_LIB		= -L$(O_PREFIX)\lib -lgomp
endif


PKGS     = libavcodec libavformat libavutil libswscale

CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  = $(shell pkg-config --libs $(PKGS))

BUILD_DIR = build

MAIN_OBJ = main.o
EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_BINS := $(patsubst examples/%.cpp, $(BUILD_DIR)/%, $(EXAMPLE_SRCS))


all: $(BUILD_DIR)/eshi examples
	echo $(dettected_OS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/eshi: $(MAIN_OBJ) shader.o | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) shader.o -o $@ $(LDFLAGS) $(OMP_LIB)

examples: $(EXAMPLE_BINS)


main.o: main.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c main.cpp -o main.o

shader.o: examples/ripple.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c examples/ripple.cpp -o shader.o

$(BUILD_DIR)/%: examples/%.cpp $(MAIN_OBJ) glsl_core.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -c $< -o $@.o
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $@.o -o $@ $(LDFLAGS) $(OMP_LIB)
	@rm $@.o

clean:
	rm -rf $(BUILD_DIR) *.o *.mp4

.PHONY: all clean examples
