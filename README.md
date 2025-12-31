# Eshi 🧑‍🎨 (絵師)

**A high-performance, hybrid CPU/GPU game engine and video generator.**

<p align="center">
<b>Hybrid Engine: Physics & UI Demo</b><br>
<img src="resources/pong.gif" style="border: 1px solid white;" alt="Pong Engine Demo" />
</p>

Eshi (Japanese for "painter" or "artist") is a minimal C++ framework that turns mathematical formulas into interactive games and high-quality video. 

It has evolved from a simple shader renderer into a **Hybrid Game Engine**, capable of running game logic, physics, and input on the CPU while rendering complex simulations on the GPU (CUDA/OpenGL) or CPU (OpenMP).

> *"Painting pixels with math."* 🥴🍹💫
>                -me
___

### 🎨 Origins & Credits
This project is heavily inspired by and builds upon the foundational concepts of **[Tzozen's](https://github.com/rexim/)** **[checker.c](https://gist.github.com/rexim/ef86bf70918034a5a57881456c0a0ccf)**.

Eshi evolves this concept by embedding a full game loop, physics system, and encoding pipeline directly into the application.

___

### ✨ Engine Features

#### 🎮 Core Systems
* **Hybrid Architecture:** Decoupled Simulation (Game Logic/Physics) from Presentation (Rendering/UI).
* **Physics System:** Built-in AABB (Axis-Aligned Bounding Box) collision detection and resolution.
* **Input Management:** Abstracted, state-based input system (Frame-perfect "Just Pressed" vs "Held" detection).
* **Game States:** Robust state machine support (Menu -> Play -> Game Over).

#### 🖥️ Rendering & Video
* **Hybrid Rendering:** Seamlessly switch between **CPU (OpenMP)**, **GPU (CUDA)**, and **GPU (OpenGL)** rendering backends.
* **UI Overlay:** Hardware-accelerated HUD and Text rendering (via SDL_ttf) layered over the simulation.
* **Zero-IO Video:** Renders directly to H.264 (`.mp4`) in memory using linked FFmpeg libraries.
* **C++ Shaders:** A robust math library (`glsl_core.h`) that emulates GLSL types (`vec2`, `vec4`) and intrinsics in standard C++.

___

### 🛠️ Build & Dependencies

#### 🐧 Linux (Debian/Ubuntu/Arch)

**1. System Libraries:**
```bash
# Core Dependencies (Updated for UI/Fonts)
sudo apt install pkg-config libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libomp-dev libsdl2-dev libsdl2-ttf-dev

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

**Troubleshooting CUDA Paths:**
If the build fails with `/usr/bin/ld: cannot find -lcudart`, your CUDA installation may be in a non-standard location. Pass the path directly to make:
```bash
make CUDA_PATH_LINUX=/opt/cuda -j$(nproc)
```

#### 🪟 Windows (x64)

**1. Prerequisites:**
* **Visual Studio** (MSVC C++ Compiler)
* **CUDA Toolkit** (Must be installed and `nvcc` reachable)
* **[vcpkg](https://vcpkg.io/)** package manager

**2. Install Dependencies:**
Open a terminal in the project root and run:
```powershell
vcpkg install --triplet x64-windows
```

**3. Configure & Compile:**
1.  Open `build_all.bat` in a text editor.
2.  Update `VCPKG_ROOT` and `CUDA_PATH` variables.
3.  Run `build_all.bat`.

#### 🐲 Windows (Arm64 / Snapdragon)

**1. Prerequisites:**
* **Visual Studio 2022** (Ensure "ARM64 build tools" are installed)
* **[vcpkg](https://vcpkg.io/)** package manager
* **OpenCL™ and OpenGL® Compatibility Pack** (Install from Microsoft Store to enable Adreno GPU support)

**2. Install Dependencies:**
Open a terminal in the project root and run:
```powershell
vcpkg install --triplet arm64-windows
```

**3. Configure & Compile:**
1.  Open the **ARM64 Native Tools Command Prompt** for VS 2022.
2.  Open `build.arm64.bat` and update `VCPKG_ROOT`.
3.  Run the build script:
    ```cmd
    build.arm64.bat
    ```

___

### 🚀 Usage

#### 1. Run the Game Engine
To launch the main engine (currently running the Pong implementation):
```bash
./build/eshi
# Or force CPU mode:
./build/eshi --cpu
```

#### 2. The Interactive Dashboard (Demos)
To browse the legacy shader demos with a GUI-like menu:
```bash
./run_demos.sh
```

#### 3. Manual CLI (Demos)
You can also run specific shader demos directly:
```bash
./build/deepsea --live   # Windowed mode
./build/deepsea          # Render to .mp4
```

**Options:**
* `--gpu`: Use hardware acceleration (CUDA on x64, OpenGL on Arm64).
* `--live`: Render to window instead of file.
* `--res WxH`: Set resolution (e.g., `--res 1920x1080`). Default is 960x540.

___

### 🧑‍💻 Writing Content
The engine separates **Simulation** from **Presentation**:

1.  **Game Logic:** Implement the `IGame` interface. Update physics and state in `Update(float dt)`.
2.  **Visuals:** Write GLSL-style C++ in your shader class to render the simulation state.
```cpp
// Example: Using GameData passed from the engine
void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime, GameData game) {
    // Draw the paddle from the physics state
    if (abs(fragCoord.x - game.paddleL.x) < 0.1f) { ... }
}
```

___

### 🏛️ License
This project retains the MIT License of the original code.
___

### 📝 Finally 
З.Ы. Если вы дочитали до этого места, буду благодарен за звезду!
___

🥂 *За здоровье!*
