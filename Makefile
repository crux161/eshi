CXX      = g++
CXXFLAGS = -fopenmp -O3 -Wall -Wextra

# Linux use -lgomp.
# macOS (Clang) user should change this to -lomp if they want to party
OMP_LIB  = -lgomp

PKGS     = libavcodec libavformat libavutil libswscale

CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS  = $(shell pkg-config --libs $(PKGS))

TARGET   = renderer
SRC      = main.cpp
DEPS     = glsl_core.h shader.cpp

# Default 
all: $(TARGET)

$(TARGET): $(SRC) $(DEPS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LDFLAGS) $(OMP_LIB)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.mp4 *.o

.PHONY: all run clean
