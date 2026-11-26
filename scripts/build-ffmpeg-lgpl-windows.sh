#!/usr/bin/env bash
# A small FFmpeg for the Windows (MSYS2) build, made here from FFmpeg's own source.
#
# It holds only what Remote Play needs to show the picture: the H.264 and HEVC decoders with
# Direct3D 11, DXVA2, Vulkan and NVIDIA hardware decoding. No GPL options are switched on, so
# FFmpeg stays under its LGPL licence, and its whole source is the one release named below. A
# copy of that source is left in SOURCES_DIR, so it can be published next to the build.
#
#   SOURCES_DIR=<folder> INSTALL_PREFIX=/mingw64 scripts/build-ffmpeg-lgpl-windows.sh
set -euo pipefail

FFMPEG_TAG="${FFMPEG_TAG:-n9.0.2}"
prefix="${INSTALL_PREFIX:-/mingw64}"
sources_dir="${SOURCES_DIR:-$PWD/corresponding-source}"
work="${FFMPEG_WORK_DIR:-$PWD/ffmpeg-src}"

pacman -S --noconfirm --needed "${MINGW_PACKAGE_PREFIX:-mingw-w64-x86_64}-ffnvcodec-headers"

if [[ ! -d "$work" ]]; then
    git clone --depth 1 --branch "$FFMPEG_TAG" https://git.ffmpeg.org/ffmpeg.git "$work" \
        || git clone --depth 1 --branch "$FFMPEG_TAG" https://github.com/FFmpeg/FFmpeg.git "$work"
fi
mkdir -p "$sources_dir"
commit="$(git -C "$work" rev-parse HEAD)"
git -C "$work" archive --format=tar.gz --prefix="ffmpeg-$FFMPEG_TAG/" HEAD > "$sources_dir/ffmpeg-$FFMPEG_TAG.tar.gz"
echo "FFmpeg $FFMPEG_TAG, commit $commit" > "$sources_dir/ffmpeg-$FFMPEG_TAG.txt"

cd "$work"
./configure --prefix="$prefix" --target-os=mingw32 --arch=x86_64 \
    --enable-shared --disable-static --disable-programs --disable-doc --disable-debug \
    --disable-everything --disable-network --disable-autodetect \
    --disable-avdevice --disable-avfilter --disable-swscale \
    --enable-avcodec --enable-avformat --enable-avutil --enable-swresample \
    --enable-w32threads \
    --enable-decoder=h264,hevc --enable-parser=h264,hevc \
    --enable-d3d11va --enable-dxva2 --enable-vulkan --enable-ffnvcodec --enable-nvdec \
    --enable-hwaccel=h264_d3d11va,h264_d3d11va2,h264_dxva2,h264_vulkan,h264_nvdec,hevc_d3d11va,hevc_d3d11va2,hevc_dxva2,hevc_vulkan,hevc_nvdec
grep -E '^(CONFIG_GPL|CONFIG_NONFREE|CONFIG_VERSION3) ' ffbuild/config.mak || true
if grep -qE '^CONFIG_(GPL|NONFREE)=yes' ffbuild/config.mak; then
    echo "error: this FFmpeg would not be LGPL" >&2
    exit 1
fi
make -j"$(nproc)"
make install
