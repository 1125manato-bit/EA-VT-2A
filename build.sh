#!/bin/bash
# Build script for VT-2A

# Add common paths to PATH
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
if [ -d "/Applications/CMake.app/Contents/bin" ]; then
    export PATH="/Applications/CMake.app/Contents/bin:$PATH"
fi

if ! command -v cmake &> /dev/null
then
    echo "CMake could not be found. Checking standard locations..."
    if [ -f "/Applications/CMake.app/Contents/bin/cmake" ]; then
        echo "Found CMake in Applications."
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    else
        echo "CMake could not be found. Please install CMake to build this project."
        echo "Brew: brew install cmake"
        echo "Download: https://cmake.org/download/"
        exit 1
    fi
fi

echo "Configuring VT-2A..."
if ! cmake -S . -B build -DCMAKE_BUILD_TYPE=Release; then
    echo "Error: Configuration failed."
    exit 1
fi

echo "Building VT-2A..."
# Use parallel build for speed
CORES=$(sysctl -n hw.ncpu)
echo "Using $CORES cores for build."
if ! cmake --build build --config Release -j "$CORES"; then
    echo "Error: Build failed."
    exit 1
fi

echo "Installing VT-2A to User Library..."

# Define paths
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
ARTIFACTS_DIR="build/VT-2A_artifacts/Release" # Adjust based on actual CMake output structure usually build/Source/VT-2A_artifacts/Release or similar

# Verify Build Output Location (JUCE CMake usually outputs to subfolder)
# Try to find the bundle (Head 1 to avoid duplicates if multiple exist)
VST3_PATH=$(find build -name "*.vst3" -type d | head -n 1)
AU_PATH=$(find build -name "*.component" -type d | head -n 1)

if [ -z "$VST3_PATH" ]; then
    echo "Error: VST3 build artifact not found. Build likely failed."
    exit 1
else
    VST3_NAME=$(basename "$VST3_PATH")
    echo "Found VST3 at $VST3_PATH ($VST3_NAME)"
    
    # Ad-hoc Code Signing (Required for macOS)
    echo "Signing VST3..."
    codesign --force --deep -s - "$VST3_PATH"

    mkdir -p "$VST3_DIR"
    rm -rf "$VST3_DIR/$VST3_NAME"
    cp -R "$VST3_PATH" "$VST3_DIR/"
    echo "Installed VST3 to $VST3_DIR"
fi

if [ -z "$AU_PATH" ]; then
    echo "Error: AU build artifact not found."
else
    AU_NAME=$(basename "$AU_PATH")
    echo "Found AU at $AU_PATH ($AU_NAME)"
    
    # Ad-hoc Code Signing
    echo "Signing AU..."
    codesign --force --deep -s - "$AU_PATH"
    
    mkdir -p "$AU_DIR"
    rm -rf "$AU_DIR/$AU_NAME"
    cp -R "$AU_PATH" "$AU_DIR/"
    echo "Installed AU to $AU_DIR"
fi

echo "Installation Complete."
echo "TIP: If it still doesn't show up, try restarting your Mac or running 'killall -9 AudioComponentRegistrar' in terminal to force a rescan."

