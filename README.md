# Sources for the Windows build aa47852

This branch holds the complete source of everything in the Windows build made from commit
aa4785257fe547b6e86893180f7d9dfb243d2c45 of this repository (the "control-pipe" branch), as the
GNU AGPL-3.0 and the licences of the libraries it ships ask:

- `chiaki-ng-control-pipe-aa47852.tar.gz`: this program's own source at that commit.
- `third-party-*.tar.gz`: the source of the libraries built into the program (git submodules).
- `ffmpeg-n9.0.2.tar.gz` and `ffmpeg-n9.0.2.txt`: FFmpeg's source and exactly how it was built.
- `libplacebo-*.tar.gz`, `hidapi-*.tar.gz`, `SDL3-*.tar.gz`, `sdl2-compat-*.tar.gz`: libraries
  built by the workflow.
- `mingw-w64-*.src.tar.zst`: MSYS2's source packages, exactly the versions installed for that
  build (`msys2-packages-installed.txt`). Files ending `.part-00`, `.part-01` were split for
  GitHub; join them with `cat name.part-* > name`.
- `build-recipe-build-msys2.yml`: the workflow that made the build.
- `SHA256SUMS.txt`: checksums of the files as gathered.

The build itself is a modified version of chiaki-ng; see MODIFIED-VERSION.txt and
THIRD-PARTY-NOTICES.txt inside it.
