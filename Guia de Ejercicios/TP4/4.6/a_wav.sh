#!/bin/sh

if [ "$#" -ne 2 ]; then
    echo "Uso: sh a_wav.sh archivo_pcm salida.wav" >&2
    exit 1
fi

ffmpeg -n -f u8 -ar 20000 -ac 2 -i "$1" -c:a pcm_u8 -f wav "$2"
