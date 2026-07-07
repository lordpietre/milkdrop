#!/bin/bash
set -e

echo "=== MilkDrop3 - Build Script ==="
echo ""

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    OS=$(uname -s)
fi

install_deps() {
    case "$OS" in
        ubuntu|debian)
            sudo apt update
            sudo apt install -y cmake g++ libsdl2-dev libglew-dev \
                                libglfw3-dev libglm-dev \
                                libgl1-mesa-dev libglu1-mesa-dev \
                                libopengl-dev libglx-dev \
                                libx11-dev libxrandr-dev libxcursor-dev \
                                libxi-dev libxinerama-dev libxxf86vm-dev
            # Optional: PipeWire para captura de audio
            sudo apt install -y libpipewire-0.3-dev 2>/dev/null || true
            ;;
        fedora)
            sudo dnf install -y cmake gcc-c++ SDL2-devel glew-devel \
                               glfw-devel glm-devel pipewire-devel
            ;;
        arch)
            sudo pacman -S --noconfirm cmake gcc sdl2 glew glfw glm pipewire
            ;;
        *)
            echo "Unknown OS: $OS. Install dependencies manually."
            ;;
    esac
}

# Parse args
if [ "$1" = "--install-deps" ]; then
    install_deps
fi

cd "$(dirname "$0")"

# Build
cmake -B build -S code
cmake --build build -j$(nproc)

echo ""
echo "=== Build complete ==="
echo "Run: cd build/vis_milk2 && ./milkdrop3"
