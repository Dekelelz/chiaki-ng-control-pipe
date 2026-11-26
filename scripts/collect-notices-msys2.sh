#!/usr/bin/env bash
# Licences and sources for a Windows (MSYS2) build of chiaki-ng, made after deploy-windows-msys2.sh.
#
# Into <package-dir> it puts COPYING, LICENSES/, a note that this is a modified chiaki-ng and
# where its source is, THIRD-PARTY-NOTICES.txt (every library file shipped, its package, version
# and licence) and licenses/<package>/ with each library's own licence files.
# Into <sources-dir> it gathers the complete source of this program and of every library it ships
# (MSYS2's source package of exactly the installed version, the bundled submodules, and the source
# of what this workflow built itself), so whoever passes the build on can offer all of it, as the
# AGPL's "Corresponding Source" asks. The GCC runtime and the MinGW thread library are left out:
# they come with the compiler (System Libraries in the AGPL's words).
#
#   scripts/collect-notices-msys2.sh <package-dir> <sources-dir> <source-url> <sources-url>
set -euo pipefail

pkg_dir="$1"
sources_dir="$2"
source_url="$3"        # this program's own source, exactly this build
sources_url="$4"       # where <sources-dir> will be published
prefix="${MINGW_PREFIX:-/mingw64}"
pfx="${MINGW_PACKAGE_PREFIX:-mingw-w64-x86_64}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "$pkg_dir/licenses" "$sources_dir"
cp "$repo_root/COPYING" "$pkg_dir/COPYING"
cp -r "$repo_root/LICENSES" "$pkg_dir/LICENSES"
cat > "$pkg_dir/MODIFIED-VERSION.txt" <<EOF
This is a modified version of chiaki-ng, not the official chiaki-ng.

The change: started with --control-pipe <name>, it takes controller input from another program
on the same PC through a local named pipe (the message format is in docs/control-pipe.md in the
source). Without that option it behaves like chiaki-ng. Please don't report problems with this
build to the chiaki-ng project.

Licence: GNU Affero General Public License, version 3 only, with the OpenSSL exception
(COPYING and LICENSES/ in this folder). You may use, copy, change and share it under that licence.

Complete source of exactly this build:
  $source_url
Source of the libraries in it (and a copy of this program's own source):
  $sources_url
EOF

notices="$pkg_dir/THIRD-PARTY-NOTICES.txt"
{
    echo "Third-party software in this build"
    echo "=================================="
    echo
    echo "This program is a modified chiaki-ng (see MODIFIED-VERSION.txt). It ships the libraries"
    echo "below, each under its own licence; their licence files are in licenses/<package>/."
    echo "The source of this program and of every library here: $sources_url"
    echo
} > "$notices"

# which installed package owns each DLL, by file name
declare -A owner=()
while read -r pkg path; do
    case "$path" in
        *.dll|*.DLL) owner["$(basename "$path" | tr '[:upper:]' '[:lower:]')"]="$pkg" ;;
    esac
done < <(pacman -Ql)

declare -A files_of=()
declare -A built_here=()
while IFS= read -r -d '' f; do
    name="$(basename "$f" | tr '[:upper:]' '[:lower:]')"
    rel="${f#"$pkg_dir"/}"
    pkg="${owner[$name]:-}"
    if [[ -n "$pkg" ]]; then
        files_of["$pkg"]+="$rel "
    else
        built_here["$rel"]=1
    fi
done < <(find "$pkg_dir" -type f \( -iname '*.dll' -o -iname '*.exe' \) -print0)


for pkg in $(printf '%s\n' "${!files_of[@]}" | sort); do
    version="$(pacman -Q "$pkg" | awk '{print $2}')"
    lic="$(pacman -Qi "$pkg" | sed -n 's/^Licenses *: *//p')"
    base="$(sed -n '/^%BASE%$/{n;p;q}' "/var/lib/pacman/local/$pkg-$version/desc")"
    short="${pkg#"$pfx"-}"
    if [[ -d "$prefix/share/licenses/$short" ]]; then
        mkdir -p "$pkg_dir/licenses/$short"
        cp -r "$prefix/share/licenses/$short/." "$pkg_dir/licenses/$short/"
    fi
    src_note=""
    # The GCC runtime and the MinGW thread library come with the compiler; every other one gets its source.
    if [[ "$short" != "gcc-libs" && "$short" != "libwinpthread" ]]; then
        src="$base-$version.src.tar.zst"
        curl -fsSL --retry 3 -o "$sources_dir/$src" "https://repo.msys2.org/mingw/sources/$src"
        src_note=" (source: $src)"
    fi
    {
        echo "- $short $version, licence: $lic$src_note"
        echo "    files: ${files_of[$pkg]}"
    } >> "$notices"
done

# What this workflow built from source itself, and chiaki-ng's own bundled libraries
add_built() {   # name, version, licence, licence file(s)...
    local name="$1" version="$2" lic="$3"
    shift 3
    mkdir -p "$pkg_dir/licenses/$name"
    local f
    for f in "$@"; do
        [[ -f "$f" ]] && cp "$f" "$pkg_dir/licenses/$name/"
    done
    echo "- $name $version, licence: $lic (built from source for this build)" >> "$notices"
}
{
    echo
    echo "Built from source for this build (their sources are with the others above, or in this"
    echo "program's own source for the ones under third-party/):"
} >> "$notices"
ffmpeg_dir="${FFMPEG_WORK_DIR:-$repo_root/ffmpeg-src}"
add_built ffmpeg "$(cat "$sources_dir"/ffmpeg-*.txt 2>/dev/null | head -1)" "LGPL-2.1-or-later" \
    "$ffmpeg_dir/LICENSE.md" "$ffmpeg_dir/COPYING.LGPLv2.1"
pl_dir="$repo_root/libplacebo"
pl_commit="$(git -C "$pl_dir" rev-parse HEAD)"
add_built libplacebo "commit $pl_commit" "LGPL-2.1-or-later" "$pl_dir/LICENSE"
tar --exclude=.git --exclude=build -czf "$sources_dir/libplacebo-$pl_commit.tar.gz" -C "$repo_root" libplacebo
for d in "$repo_root"/SDL3-*/; do
    [[ -d "$d" ]] && add_built "SDL3" "$(basename "$d")" "Zlib" "$d/LICENSE.txt"
done
for d in "$repo_root"/sdl2-compat*/; do
    [[ -d "$d" ]] && add_built "sdl2-compat" "$(basename "$d")" "Zlib" "$d/LICENSE.txt"
done
[[ -d "$repo_root/hidapi" ]] && add_built hidapi "$(git -C "$repo_root/hidapi" describe --tags 2>/dev/null || echo '?')" \
    "BSD-3-Clause (one of its three licence choices)" "$repo_root/hidapi/LICENSE-bsd.txt" "$repo_root/hidapi/LICENSE.txt"
for d in "$repo_root"/third-party/*/; do
    n="$(basename "$d")"
    lic_files=()
    for f in "$d"LICENSE* "$d"COPYING* "$d"license* ; do [[ -f "$f" ]] && lic_files+=("$f"); done
    sub_commit="$(git -C "$d" rev-parse HEAD 2>/dev/null || echo unknown)"
    add_built "$n" "commit $sub_commit" "see licenses/$n" "${lic_files[@]}"
    tar --exclude=.git -czf "$sources_dir/third-party-$n-$sub_commit.tar.gz" -C "$repo_root/third-party" "$n"
done
for f in "$repo_root"/SDL3-*.tar.gz "$repo_root"/sdl2-compat-*.tar.gz; do
    [[ -f "$f" ]] && cp "$f" "$sources_dir/"
done
if [[ -d "$repo_root/hidapi" ]]; then
    tar --exclude=.git --exclude=build -czf "$sources_dir/hidapi-$(git -C "$repo_root/hidapi" rev-parse HEAD).tar.gz" -C "$repo_root" hidapi
fi
# this program's own source, exactly this build (also on its repository page)
git -C "$repo_root" archive --format=tar.gz --prefix="chiaki-ng-control-pipe/" HEAD > "$sources_dir/chiaki-ng-control-pipe-$(git -C "$repo_root" rev-parse --short=7 HEAD).tar.gz"

{
    echo
    echo "Files not matched to a package (should be only this program and what is listed above):"
    for rel in $(printf '%s\n' "${!built_here[@]}" | sort); do echo "    $rel"; done
} >> "$notices"

( cd "$sources_dir" && sha256sum * > SHA256SUMS.txt )
echo "notices written; sources gathered:"
ls -la "$sources_dir"
