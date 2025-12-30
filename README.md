# Eshi 🧑‍🎨 (絵師)

**A high-performance, hybrid CPU/GPU shader engine and video generator.**

### 🖼️ Gallery
||||
|:---:|:---:|:---:|
| **Deep Sea**<br>![Deep Sea](resources/deepsea.gif) | **Fractal**<br>![Fractal](resources/fractal.gif) | **Polar**<br>![Polar](resources/polar.gif) |
| **Raymarch**<br>![Raymarch](resources/raymarch.gif) | **Ripple**<br>![Ripple](resources/ripple.gif) | **Starfield**<br>![Starfield](resources/starfield.gif) |
| **Tzozen**<br>![Tzozen](resources/tzozen.gif) | **Voronoi**<br>![Voronoi](resources/voronoi.gif) | **Bubbles**<br>![Bubbles](resources/bubbles.gif) |

Eshi (Japanese for "painter" or "artist") is a minimal C++ framework that turns mathematical formulas into video. It allows you to write GLSL-style logic directly in C++, rendering procedural art to high-quality video files (`.mp4`) or a live window preview.

> *"Painting pixels with math."* 🥴🍹💫
>                -me
___

### 🎨 Origins & Credits
This project is heavily inspired by and builds upon the foundational concepts of **[Tzozen's](https://github.com/rexim/)** **[checker.c](https://gist.github.com/rexim/ef86bf70918034a5a57881456c0a0ccf)**.

Eshi evolves this concept by embedding the encoding pipeline directly into the application, supporting multi-threaded CPU rendering (OpenMP), GPU acceleration (CUDA), and live previews (SDL2).

___

### ✨ Features
* **Hybrid Rendering:** Seamlessly switch between **CPU (OpenMP)** and **GPU (CUDA)** rendering engines.
* **Live Preview:** Tweak your shaders in real-time with an SDL2 window (`--live`).
* **Zero-IO Video:** Renders directly to H.264 (`.mp4`) in memory using linked FFmpeg libraries.
* **Modern CLI:** Beautiful terminal UI powered by [Gum](https://github.com/charmbracelet/gum).
* **C++ Shaders:** A robust math library (`glsl_core.h`) that emulates GLSL types (`vec2`, `vec4`) and intrinsics in standard C++.

___

### 🛠️ Build & Dependencies

**1. System Libraries (Debian/Ubuntu):**
```bash
# Core Dependencies
sudo apt install pkg-config libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libomp-dev libsdl2-dev

# Install Gum (for the CLI dashboard)
sudo mkdir -p /etc/apt/keyrings
curl -fsSL [https://repo.charm.sh/apt/gpg.key](https://repo.charm.sh/apt/gpg.key) | sudo gpg --dearmor -o /etc/apt/keyrings/charm.gpg
echo "deb [signed-by=/etc/apt/keyrings/charm.gpg] [https://repo.charm.sh/apt/](https://repo.charm.sh/apt/) * *" | sudo tee /etc/apt/sources.list.d/charm.list
sudo apt update && sudo apt install gum
```

**2. Compile:**
```bash
make -j$(nproc)
```

*(Note: If `nvcc` is found in your PATH, Eshi automatically compiles with CUDA support.)*

___

### 🚀 Usage

#### 1. The Interactive Dashboard (Recommended)
The easiest way to run demos is with the beautified shell script:
```bash
./run_demos.sh
```
This launches a GUI-like menu in your terminal to select CPU/GPU modes and shows progress bars for renders.

#### 2. Manual CLI
You can also run the built binaries directly from the `build/` folder:

**Render to Video:**
```bash
./build/deepsea
# Output: deepsea.mp4 (in build/)
```

**Live Preview:**
```bash
./build/deepsea --live
# Opens a window. Press ESC to close.
```

**Options:**
* `--gpu`: Use CUDA rendering engine (if compiled).
* `--live`: Render to window instead of file.
* `--res WxH`: Set resolution (e.g., `--res 1920x1080`). Default is 960x540.

___

### 🧑‍💻 Writing Shaders
Define your art in `shader.cpp` using the `mainImage` function (Shadertoy style):
```cpp
void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    // ... your GLSL logic here ...
    fragColor = vec4(uv.x, uv.y, 0.5f + 0.5f*sinf(iTime), 1.0f);
}
```

Rebuild with `make` to update the binary.

___

### 🏛️ License
This project retains the MIT License of the original code.
___

### 📝 Finally 
З.Ы. Если вы дочитали до этого места, буду благодарен за звезду!
___

🥂 *За здоровье!*
