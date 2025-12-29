#!/bin/bash

# 1. Use /bin/bash to support TIMEFORMAT and [[ ]] logic properly.
# 2. Define directory relative to the script location, not where you run it from.
DIR="$(cd "$(dirname "$0")" && pwd)/build"
ARGS=""
GPU_ID=""

# --- PARSE ARGUMENTS ---
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --gpu)
            # Pass the boolean flag to the C++ executable
            ARGS="--gpu"
            
            # Optional: If the user provides a number (e.g. --gpu 0), use it for CUDA
            # This allows you to pick a specific card if you have multiple.
            if [[ -n "$2" ]] && [[ "$2" != --* ]]; then
                GPU_ID="$2"
                shift # Consume the value
            fi
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
    shift # Consume the flag
done

# --- RUN LOGIC ---
if [ ! -d "$DIR" ]; then
    echo "Error: Build directory not found at $DIR"
    echo "Please run 'make' first."
    exit 1
fi

echo "Starting demos in: $DIR"
if [[ -n "$ARGS" ]]; then
    echo "Mode: GPU Accelerated"
    if [[ -n "$GPU_ID" ]]; then
        echo "Targeting CUDA Device: $GPU_ID"
        export CUDA_VISIBLE_DEVICES="$GPU_ID"
    fi
else
    echo "Mode: CPU (OpenMP)"
fi

# Define the run function
run_suite() {
    # Move into build dir so executables find their assets if needed
    cd "$DIR" || exit 1

    # Set bash time format to just the seconds
    TIMEFORMAT="Time: %3lR seconds"

    # Run sequentially. Stop if one fails (&&).
    # We pass $ARGS (which is empty for CPU, or "--gpu" for GPU)
    time ./deepsea $ARGS && \
    time ./fractal $ARGS && \
    time ./polar $ARGS && \
    time ./raymarch $ARGS && \
    time ./ripple $ARGS && \
    time ./starfield $ARGS && \
    time ./tzozen $ARGS && \
    time ./voronoi $ARGS
}

# --- EXECUTION & TIMING ---
start_time=$(date +%s%3N)

# Run the suite
run_suite

# Check if the suite succeeded
if [ $? -eq 0 ]; then
    end_time=$(date +%s%3N)
    duration=$((end_time - start_time))
    # Calculate seconds with 3 decimal places
    hr_duration=$(echo "scale=3; $duration / 1000" | bc)
    
    echo "--------------------------------------"
    echo "All demos finished in: $hr_duration seconds"
else
    echo "--------------------------------------"
    echo "Demo suite failed or was interrupted."
fi
