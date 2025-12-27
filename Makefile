CXX      = g++
CXXFLAGS = -I. -fopenmp -O3 -Wall -Wextra -flto -DLINK_SHADER
OMP_LIB  = -lgomp

PKGS     = libavcodec libavformat libavutil libswscale
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  = $(shell pkg-config --libs $(PKGS))

BUILD_DIR = build

MAIN_OBJ = main.o
EXAMPLE_SRCS := $(wildcard examples/*.cpp)
EXAMPLE_BINS := $(patsubst examples/%.cpp, $(BUILD_DIR)/%, $(EXAMPLE_SRCS))


all: $(BUILD_DIR)/eshi examples

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/eshi: $(MAIN_OBJ) shader.o | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) shader.o -o $@ $(LDFLAGS) $(OMP_LIB)

examples: $(EXAMPLE_BINS)


main.o: main.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

shader.o: examples/ripple.cpp glsl_core.h
	$(CXX) $(CXXFLAGS) -c examples/ripple.cpp -o shader.o

$(BUILD_DIR)/%: examples/%.cpp $(MAIN_OBJ) glsl_core.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@.o
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $@.o -o $@ $(LDFLAGS) $(OMP_LIB)
	@rm $@.o

clean:
	rm -rf $(BUILD_DIR) *.o *.mp4

.PHONY: all clean examples
