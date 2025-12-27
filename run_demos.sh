#!/bin/sh

dir="$(pwd)/build"

demos () {
    TIMEFORMAT="time: $2R seconds"
    cd ./build
    time ./deepsea && \
    time ./fractal && \
    time ./polar && \
    time ./raymarch && \
    time ./ripple && \
    time ./starfield && \
    time ./tzozen && \
    time ./voronoi
}

if [ -d "$dir" ]; then
    echo "Starting demos..."
    start_time=$(date +%s%3N)
    demos
    end_time=$(date +%s%3N)
    durration=$((end_time - start_time))
    hr_durration=$(echo "scale=3; $durration / 1000" | bc)
    echo "Finished in: $hr_durration seconds"
else
    echo "Build directory not found. Please run 'make' first."
fi
