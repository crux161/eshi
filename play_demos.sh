#!/bin/sh
#

if command -v mpv &> /dev/null
then
	mpv ./build/*.mp4
else
	echo "mpv is not found in your path."
fi
