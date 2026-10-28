#!/bin/sh
# Build a self-contained Steam Frame release archive on an ARM64 Frame.
set -eu

fc_source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fc_version="${1:-dev}"
case "$fc_version" in *[!A-Za-z0-9._-]*|'') printf 'Invalid version.\n' >&2; exit 2 ;; esac
[ "$(uname -m)" = aarch64 ] || { printf 'Build this release on Steam Frame ARM64.\n' >&2; exit 1; }
command -v strip >/dev/null 2>&1 || { printf 'Missing required command: strip\n' >&2; exit 1; }

cmake -G Ninja -S "$fc_source_dir" -B "$fc_source_dir/build"
cmake --build "$fc_source_dir/build"
fc_layer_build="$fc_source_dir/build/vkbasalt"
# Release build: meson defaults to an unoptimized debug build (see install.sh)
if [ ! -f "$fc_layer_build/build.ninja" ]; then
    meson setup --buildtype=release "$fc_layer_build" "$fc_source_dir/third_party/vkbasalt"
else
    meson setup --wipe --buildtype=release "$fc_layer_build" "$fc_source_dir/third_party/vkbasalt"
fi
ninja -C "$fc_layer_build"

fc_temp_dir=$(mktemp -d)
trap 'rm -rf "$fc_temp_dir"' EXIT HUP INT TERM
fc_stage_dir="$fc_temp_dir/framecolor"
install -Dm755 "$fc_source_dir/build/framecolor" "$fc_stage_dir/bin/framecolor"
install -Dm755 "$fc_layer_build/src/libvkbasalt.so" "$fc_stage_dir/lib/libvkbasalt.so"
strip --strip-debug "$fc_stage_dir/bin/framecolor" "$fc_stage_dir/lib/libvkbasalt.so"
install -Dm644 "$fc_source_dir/THIRD_PARTY_LICENSES.md" "$fc_stage_dir/THIRD_PARTY_LICENSES.md"
install -Dm644 "$fc_source_dir/THIRD_PARTY_LICENSES.ja.md" "$fc_stage_dir/THIRD_PARTY_LICENSES.ja.md"
install -Dm644 "$fc_source_dir/LICENSE" "$fc_stage_dir/LICENSE"
for fc_license in \
    third_party/frameeyeosc-LICENSE \
    third_party/openvr/LICENSE \
    third_party/vkbasalt/LICENSE; do
    install -Dm644 "$fc_source_dir/$fc_license" "$fc_stage_dir/$fc_license"
done
install -Dm644 "$fc_source_dir/tools/generate_vulkan_lut.py" "$fc_stage_dir/tools/generate_vulkan_lut.py"
for fc_name in install.sh uninstall.sh framecolor-guard framecolorctl framecolor.service 90-framecolor.conf; do
    install -Dm644 "$fc_source_dir/contrib/$fc_name" "$fc_stage_dir/contrib/$fc_name"
done
install -d "$fc_source_dir/dist"
fc_archive_name="framecolor-$fc_version-steamframe-aarch64.tar.gz"
fc_archive="$fc_source_dir/dist/$fc_archive_name"
tar -czf "$fc_archive" -C "$fc_temp_dir" framecolor
(cd "$fc_source_dir/dist" && sha256sum "$fc_archive_name" > "$fc_archive_name.sha256")
printf 'Created %s and %s.sha256\n' "$fc_archive" "$fc_archive"
