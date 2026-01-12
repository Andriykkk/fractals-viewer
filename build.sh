#!/bin/bash

BUILD_DIR="build"
EXECUTABLE="fractal_viewer"

build() {
    echo "Building..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ..
    make -j$(nproc)
    cd ..
}

run() {
    if [ ! -f "$BUILD_DIR/$EXECUTABLE" ]; then
        echo "Executable not found. Building first..."
        build
    fi
    echo "Running..."
    ./"$BUILD_DIR/$EXECUTABLE"
}

clean() {
    echo "Cleaning..."
    rm -rf "$BUILD_DIR"
    echo "Done."
}

usage() {
    echo "Usage: $0 {build|run|br|clean}"
    echo "  build  - Build the project"
    echo "  run    - Run the project (builds if needed)"
    echo "  br     - Build and run"
    echo "  clean  - Remove build directory"
}

case "$1" in
    build)
        build
        ;;
    run)
        run
        ;;
    br)
        build && run
        ;;
    clean)
        clean
        ;;
    *)
        usage
        exit 1
        ;;
esac
