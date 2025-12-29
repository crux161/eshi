#!/bin/bash

mkdir -p logs

if [ ! -d "build" ]; then
    echo "❌ Error: Build directory not found. Run 'make' first."
    exit 1
fi
cd build || exit 1

DEMOS=("deepsea" "neon" "bubbles" "fractal" "polar" "raymarch" "ripple" "starfield" "tzozen" "voronoi")
TOTAL_FRAMES_PER_DEMO=240
TOTAL_DEMOS=${#DEMOS[@]}
ARGS=""


if ! command -v gum &> /dev/null; then
    echo "❌ Gum is not installed."
    exit 1
fi


COLOR_BAR="\033[38;5;212m"
COLOR_BG="\033[38;5;240m"
COLOR_TEXT="\033[38;5;255m"
COLOR_SUCCESS="\033[38;5;42m"
COLOR_ERROR="\033[38;5;196m"
RESET="\033[0m"


draw_progress() {
    local name=$1
    local frame=$2
    local demo_idx=$3

    local pct=$(( frame * 100 / TOTAL_FRAMES_PER_DEMO ))
    if [ $pct -gt 100 ]; then pct=100; fi

    local width=30
    local filled=$(( pct * width / 100 ))
    local empty=$(( width - filled ))

    local bar_filled=$(printf "%0.s█" $(seq 1 $filled))
    local bar_empty=$(printf "%0.s░" $(seq 1 $empty))

    local overall="$((demo_idx + 1))/$TOTAL_DEMOS"

    printf "\r${COLOR_TEXT}%-12s ${COLOR_BAR}%s${COLOR_BG}%s${RESET} %3d%% ${COLOR_BG}(%s)${RESET}\033[K" \
        "$name" "$bar_filled" "$bar_empty" "$pct" "$overall"
}


gum style --border normal --margin "1" --padding "1 2" --border-foreground 212 "🎨 Eshi Render Suite"

MODE=$(gum choose "CPU (OpenMP)" "GPU (CUDA)" --header "Select Rendering Engine")

if [ "$MODE" == "GPU (CUDA)" ]; then
    ARGS="--gpu"
fi

echo ""
gum style --foreground 212 "Starting render engine..."
echo ""


start_time=$(date +%s%3N)

for i in "${!DEMOS[@]}"; do
    demo="${DEMOS[$i]}"


    LOG_FILE="../logs/${demo}.log"


    stdbuf -oL ./"$demo" $ARGS 2> "$LOG_FILE" | tr '\r' '\n' | \
    while read -r line; do
        if [[ "$line" =~ Frame\ ([0-9]+) ]]; then
            frame_num="${BASH_REMATCH[1]}"
            draw_progress "$demo" "$frame_num" "$i"
        fi
    done

    exit_codes=("${PIPESTATUS[@]}")
    binary_exit_code=${exit_codes[0]}

    if [ $binary_exit_code -ne 0 ]; then
        echo ""
        echo -e "${COLOR_ERROR}❌ Failed to render $demo${RESET}"
        gum style --foreground 240 "ERROR LOG ($LOG_FILE):"
        if [ -f "$LOG_FILE" ]; then cat "$LOG_FILE"; fi
        exit 1
    else
        printf "\r${COLOR_SUCCESS}✓ %-12s${RESET} %s\033[K\n" "$demo" "Rendered"
    fi
done


end_time=$(date +%s%3N)
duration=$((end_time - start_time))
hr_duration=$(echo "scale=3; $duration / 1000" | bc)

echo ""
gum style \
	--border double \
	--margin "1" \
	--padding "1 2" \
	--border-foreground 212 \
	--foreground 255 \
	"✨ All demos finished in ${hr_duration}s"
