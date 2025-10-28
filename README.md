# mySTLViewer

A modern OpenGL-based STL file viewer built with CMake, vcpkg, SDL3, and OpenGL 3.3+.

![Solid rendering example](screenshots/solid.png)

## Features

- **STL File Support**: Reads both ASCII and binary STL files
- **XML/ZIP Geometry Files**: Supports XML-based geometry definitions and ZIP archives containing XML
- **Modern OpenGL**: Uses OpenGL 3.3+ with programmable shaders
- **Flexible Rendering Modes**: Independent toggles for solid fill and wireframe overlay
  - Solid-only mode with flat per-facet shading
  - Wireframe-only mode (white lines)
  - Combined mode (solid with black wireframe overlay)
- **Advanced Triangulation**: Robust polygon triangulation with plane projection and automatic fallback
- **Back-face Culling**: Optional culling toggle (default: OFF)
- **Debug Visualization**: Toggle overlays for facet normals, triangle normals, and triangle edges
- **Orthogonal Projection**: Clean orthographic view of 3D models
- **Interactive Controls**: Mouse-based rotation, pan, zoom, custom rotation pivot, and light source rotation
- **Kinetic Motion**: Optional inertial rotation and zoom for smooth interaction
- **Startup File Dialog**: Native file chooser on launch (argument optional)
- **Cross-platform**: Built with CMake and vcpkg for easy portability

## Prerequisites

- CMake 3.20 or higher
- vcpkg package manager
- C++17 compatible compiler
- Git

## Building the Project

### 1. Install vcpkg (if not already installed)

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # On macOS/Linux
# ./bootstrap-vcpkg.bat  # On Windows
```

### 2. Set vcpkg environment variable (optional but recommended)

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

### 3. Build the project

```bash
# From the project root directory
mkdir build
cd build

# Configure with CMake (specify vcpkg toolchain)
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Or if you didn't set VCPKG_ROOT:
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build .
```

### 4. Run the application

```bash
./mySTLViewer
```

On startup, a native file dialog will prompt you to select an STL file. You can also pass a file path as a command-line argument to skip the dialog.

## Usage

### Command Line

```bash
mySTLViewer [geometry_file]
```

- If `[geometry_file]` is provided, the viewer opens it directly (supports `.stl`, `.xml`, `.zip`)
- If omitted, a native file dialog appears to select a geometry file
- When launched by double-clicking (e.g., in macOS Finder), working directory automatically changes to executable location for proper shader loading

### Controls

#### Mouse Controls
- **Right Mouse Button + Drag**: Rotate the model
- **Left Mouse Button (click)**: Pick a rotation pivot (closest vertex under cursor); axes appear at the pivot
- **Middle Mouse Button + Drag**: Pan the view
- **Mouse Wheel**: Zoom in/out

#### Trackpad Alternatives
- **Z + Left Mouse Drag**: Zoom in/out (alternative to mouse wheel)
- **D + Left Mouse Drag**: Pan the view (alternative to middle mouse)

#### Keyboard Shortcuts

##### Rendering Modes
- **W**: Toggle wireframe overlay (white when alone, black when overlaid on solid)
- **S**: Toggle solid fill (flat per-facet shading)
- **C**: Toggle back-face culling (default: OFF)
- **N**: Toggle debug normals overlay
  - Facet normals (magenta lines)
  - Triangle normals (cyan lines)
  - Triangle edges from triangulation (yellow lines)

##### View Controls
- **R**: Reset view to default position, clear custom pivot, and reset light direction
- **K**: Toggle kinetic rotate/zoom (inertia; pan is not affected)
- **V**: Toggle VSync

##### Lighting Controls
- **L + Right Mouse Drag**: Rotate the light source direction

##### File Operations
- **Ctrl/Cmd + O**: Open file dialog to load a new geometry file
- **Q** or **ESC**: Quit application
- **Ctrl/Cmd + Q**: Quit application

##### Performance
- **M**: Toggle OpenMP-based pivot picking (if enabled in build)

#### Custom Pivot Notes
- When a pivot is selected, rotations occur around that point; otherwise the model rotates around its center
- If you click far from the model (more than ~100 px from any vertex), pivot mode is disabled automatically
- Selecting a pivot does not shift the view; the chosen point stays under the cursor
- Colored axes (RGB = XYZ) appear at the selected pivot point

#### Kinetic Controls
- Disabled by default; toggle with the **K** key
- Rotation inertia continues after you release the right mouse button, based on your last drag velocity
- Zoom inertia continues briefly after a mouse wheel scroll
- Panning is never affected by kinetic motion
- Adjustable damping keeps motion smooth and natural

## Project Structure

```
mySTLViewer/
├── CMakeLists.txt          # CMake build configuration
├── vcpkg.json              # vcpkg dependencies
├── README.md               # This file
├── src/
│   ├── main.cpp            # Application entry point and SDL3 setup
│   ├── STLLoader.h/.cpp    # STL file parser (binary & ASCII)
│   ├── XMLLoader.h/.cpp    # XML/ZIP geometry file parser
│   ├── Renderer.h/.cpp     # OpenGL rendering engine
│   └── Mesh.h              # Mesh data structure
└── shaders/
    ├── vertex.glsl         # Vertex shader
    ├── fragment.glsl       # Fragment shader with flat shading
    └── solid.geom          # Geometry shader for per-facet flat shading
```

## Dependencies

The project uses the following libraries managed by vcpkg:

- **SDL3**: Window management and input handling
- **glad**: OpenGL function loader
- **glm**: OpenGL Mathematics library for matrix operations
- **nativefiledialog-extended (nfd)**: Native file open dialog on all platforms
- **libarchive**: ZIP archive extraction for XML geometry files
- **pugixml**: XML parsing for geometry definitions
- **OpenMP (optional)**: Speeds up pivot picking by parallelizing the nearest-vertex search
    - On macOS with AppleClang, install `libomp` via Homebrew; the build auto-detects and links it

## Technical Details

### STL File Format Support

- **Binary STL**: Fast loading, compact file size
- **ASCII STL**: Human-readable format
- Auto-detection of file format

### XML Geometry Support

- **XML files**: Custom XML-based geometry definitions with facet data
- **ZIP archives**: Automatic extraction and parsing of XML files within ZIP containers
- Support for complex polygons with automatic triangulation

### Rendering Features

- **Independent Rendering Modes**:
  - Solid fill with true flat per-facet shading (using geometry shader)
  - Wireframe overlay (adaptive color: white standalone, black overlay)
  - Both modes can be active simultaneously
- **Advanced Triangulation**:
  - Plane-projected polygon triangulation using earcut
  - Automatic triangle orientation correction to match facet normals
  - Triangle fan fallback for degenerate cases
- **Debug Visualization**:
  - Facet normals (magenta lines from facet centroids)
  - Triangle normals (cyan lines from triangle centroids)
  - Triangle edges (yellow lines showing triangulation structure)
- Orthogonal projection for accurate size representation
- View-space lighting with ambient and diffuse components
- Interactive light source rotation (L + right drag)
- Automatic model centering and scaling
- Optional back-face culling (default: OFF)
- Custom rotation pivot with visual axes (RGB = XYZ)

### OpenGL Features

- Modern OpenGL 3.3 Core Profile (required)
- Vertex Array Objects (VAO)
- Vertex Buffer Objects (VBO)
- Element Buffer Objects (EBO)
- GLSL Shaders (version 330)
- Geometry shader for true per-facet flat shading
- Multi-pass rendering (solid + wireframe overlay)
- Depth testing and back-face culling support

## Troubleshooting

### vcpkg package installation fails

Make sure vcpkg is up to date:
```bash
cd $VCPKG_ROOT
git pull
./bootstrap-vcpkg.sh
```

### OpenGL version errors

Ensure your graphics drivers are up to date and support OpenGL 3.3 or higher. The application checks and exits early if the runtime OpenGL version is below 3.3.

### Shader compilation errors

Check that the `shaders/` directory is in the same location as the executable. When double-clicking the executable in macOS Finder, the working directory is automatically adjusted to the executable's location. CMake copies shaders to the build directory automatically.

### Model appears transparent or has missing faces

- Toggle back-face culling with **C** (default is OFF)
- The triangulation system automatically corrects triangle orientation to match facet normals
- Use **N** to visualize facet normals (magenta) and verify geometry

### Enable OpenMP (parallel pivot picking) on macOS

AppleClang doesn’t ship OpenMP by default. This project can enable OpenMP via Homebrew’s `libomp` on Apple Silicon:

1) Install libomp
```bash
brew install libomp
```

2) Configure the project (from the `build/` dir)
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

The build will try the standard OpenMP detection first. If that fails on macOS, it will fall back to Homebrew’s libomp at `/opt/homebrew/opt/libomp` automatically and define `HAVE_OPENMP`.

If your Homebrew prefix is different, set it explicitly:
```bash
cmake .. -DHOMEBREW_LIBOMP_PREFIX=/custom/prefix/opt/libomp \
         -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

3) At runtime, press `M` to toggle OpenMP-based picking on/off.

## License

This project is provided as-is for educational and personal use.

## Future Enhancements

Potential features for future development:

- Support for additional file formats (OBJ, PLY, 3MF)
- Model measurements and analysis tools
- Material/color customization per facet
- Screenshot/export functionality
- Model slicing preview
- Performance optimizations for very large models (LOD, instancing)
- Animation/turntable mode
- Section plane cutting tools
