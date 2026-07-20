#!/bin/bash


if ! command -v mpv &> /dev/null; then
    echo "Error: mpv is not found in your path."
    exit 1
fi

if ! command -v gum &> /dev/null; then
    echo "Error: gum is not installed."
    exit 1
fi

cd ./build || exit 1


FILES=$(find . -maxdepth 1 -name "*.mp4" | sed 's|./||')

if [ -z "$FILES" ]; then
    gum style --foreground 196 "No video files found in build/."
    exit 1
fi



gum style --foreground 212 "Select videos to play (TAB to select multiple, ENTER to run)"
SELECTED=$(echo "$FILES" | gum choose --no-limit --height 10)

if [ -z "$SELECTED" ]; then
    echo "No files selected."
    exit 0
fi


echo "$SELECTED" | while read -r file; do
    if [ -n "$file" ]; then
        gum style --foreground 99 "▶️  Playing $file..."
        mpv --loop "$file" > /dev/null 2>&1
    fi
done
