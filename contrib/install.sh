#!/bin/sh
# Install entirely in the invoking user's home directory. No sudo or network.
set -eu

fc_source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fc_config_dir="$HOME/.config/framecolor"
fc_data_dir="$HOME/.local/share/framecolor"
fc_unit_dir="$HOME/.config/systemd/user"
fc_bin_dir="$HOME/.local/bin"
fc_libexec_dir="$HOME/.local/libexec"

if [ "$(id -u)" -eq 0 ]; then
    printf 'Run this as the normal Steam Frame user, without sudo.\n' >&2
    exit 1
fi
if [ "$(uname -m)" != aarch64 ] || [ ! -d /opt/steamvr ]; then
    printf 'This release is for Steam Frame (SteamOS ARM64) only.\n' >&2
    exit 1
fi
for fc_command in install systemctl steamvr python3; do
    command -v "$fc_command" >/dev/null 2>&1 || { printf 'Missing required command: %s\n' "$fc_command" >&2; exit 1; }
done
fc_dashboard_was_installed=false
fc_dashboard_was_enabled=false
if [ -f "$fc_unit_dir/framecolor.service" ]; then
    fc_dashboard_was_installed=true
    if systemctl --user is-enabled --quiet framecolor.service; then
        fc_dashboard_was_enabled=true
    fi
fi

if [ -x "$fc_source_dir/bin/framecolor" ] && [ -f "$fc_source_dir/lib/libvkbasalt.so" ]; then
    fc_overlay_binary="$fc_source_dir/bin/framecolor"
    fc_layer_binary="$fc_source_dir/lib/libvkbasalt.so"
else
    for fc_command in cmake meson ninja pkg-config glslangValidator; do
        command -v "$fc_command" >/dev/null 2>&1 || {
            printf 'No prebuilt binaries found; source build requires %s. Use the GitHub release archive.\n' "$fc_command" >&2
            exit 1
        }
    done
    cmake -G Ninja -S "$fc_source_dir" -B "$fc_source_dir/build"
    cmake --build "$fc_source_dir/build"
    fc_overlay_binary="$fc_source_dir/build/framecolor"
    fc_layer_build="$fc_source_dir/build/vkbasalt"
    # The layer runs inside vrcompositor on every frame. Meson defaults to an
    # unoptimized debug build, so ask for a release build.
    if [ ! -f "$fc_layer_build/build.ninja" ]; then
        meson setup --buildtype=release "$fc_layer_build" "$fc_source_dir/third_party/vkbasalt"
    else
        meson setup --wipe --buildtype=release "$fc_layer_build" "$fc_source_dir/third_party/vkbasalt"
    fi
    ninja -C "$fc_layer_build"
    fc_layer_binary="$fc_layer_build/src/libvkbasalt.so"
fi

install -Dm755 "$fc_overlay_binary" "$fc_bin_dir/framecolor"
install -Dm755 "$fc_layer_binary" "$fc_data_dir/lib/libvkbasalt.so"
install -Dm755 "$fc_source_dir/contrib/framecolor-guard" "$fc_libexec_dir/framecolor-guard"
install -Dm755 "$fc_source_dir/contrib/framecolorctl" "$fc_bin_dir/framecolorctl"
install -Dm644 "$fc_source_dir/contrib/framecolor.service" "$fc_unit_dir/framecolor.service"
install -Dm644 "$fc_source_dir/contrib/90-framecolor.conf" "$fc_unit_dir/steamvr.service.d/90-framecolor.conf"
install -d -m 700 "$fc_config_dir"
install -d -m 755 "$fc_data_dir/layer"

if [ ! -f "$fc_config_dir/current.txt" ]; then
    printf '0 0 0 1 1 1 1 1 1 1 1 1\n' > "$fc_config_dir/current.txt"
fi
python3 "$fc_source_dir/tools/generate_vulkan_lut.py" "$fc_config_dir/current.txt" "$fc_data_dir/framecolor.cube"

python3 - "$fc_data_dir" <<'PY'
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
manifest = {
    "file_format_version": "1.0.0",
    "layer": {
        "name": "VK_LAYER_VKBASALT_post_processing",
        "type": "GLOBAL",
        "library_path": str(root / "lib/libvkbasalt.so"),
        "api_version": "1.3.223",
        "implementation_version": "1",
        "description": "FrameColor Vulkan correction for Steam Frame",
        "functions": {
            "vkGetInstanceProcAddr": "vkBasalt_GetInstanceProcAddr",
            "vkGetDeviceProcAddr": "vkBasalt_GetDeviceProcAddr",
        },
        "enable_environment": {"ENABLE_VKBASALT": "1"},
        "disable_environment": {"DISABLE_VKBASALT": "1"},
    },
}
(root / "layer/vkBasalt.json.disabled").write_text(json.dumps(manifest, indent=2) + "\n")
(root / "vkBasalt.conf").write_text(
    f"lutFile = {root / 'framecolor.cube'}\n"
)
PY

printf 'VK_ADD_IMPLICIT_LAYER_PATH=%s/layer\n' "$fc_data_dir" > "$fc_config_dir/vulkan.env"
printf 'ENABLE_VKBASALT=1\n' >> "$fc_config_dir/vulkan.env"
printf 'VKBASALT_CONFIG_FILE=%s/vkBasalt.conf\n' "$fc_data_dir" >> "$fc_config_dir/vulkan.env"
printf 'VKBASALT_LOG_LEVEL=warn\n' >> "$fc_config_dir/vulkan.env"

systemctl --user daemon-reload
if [ "$fc_dashboard_was_installed" = false ] || [ "$fc_dashboard_was_enabled" = true ]; then
    systemctl --user enable --now framecolor.service
    if [ "$fc_dashboard_was_installed" = true ]; then
        systemctl --user restart framecolor.service
    fi
else
    printf 'Preserved the disabled dashboard service. Use framecolorctl enable to start it.\n'
fi
if [ -f "$fc_data_dir/layer/vkBasalt.json" ]; then
    systemctl --user restart steamvr.service
    printf 'Updated the enabled Vulkan layer and restarted SteamVR.\n'
else
    printf 'Installed in your home directory. Vulkan correction is disabled until you run:\n'
    printf '  %s/framecolorctl enable\n' "$fc_bin_dir"
fi
printf 'To undo the entire installation, run: sh %s/contrib/uninstall.sh\n' "$fc_source_dir"
