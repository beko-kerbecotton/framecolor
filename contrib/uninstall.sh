#!/bin/sh
# Remove files installed by FrameColor, including saved presets/settings.
# Does not touch the SteamVR runtime or any OS-owned files.
set -eu

fc_config_root="$HOME/.config"
fc_config_dir="$fc_config_root/framecolor"
fc_data_root="$HOME/.local/share"
fc_data_dir="$fc_data_root/framecolor"
fc_unit_dir="$fc_config_root/systemd/user"
fc_manifest="$fc_data_dir/layer/vkBasalt.json"
fc_was_enabled=false

if [ -f "$fc_manifest" ]; then
    mv "$fc_manifest" "$fc_manifest.disabled"
    fc_was_enabled=true
fi
systemctl --user disable --now framecolor.service >/dev/null 2>&1 || true

rm -f "$fc_unit_dir/framecolor.service" "$fc_unit_dir/steamvr.service.d/90-framecolor.conf"
rm -f "$HOME/.local/bin/framecolor" "$HOME/.local/bin/framecolorctl"
rm -f "$HOME/.local/libexec/framecolor-guard"
rm -f "$fc_data_dir/layer/vkBasalt.json.disabled" "$fc_data_dir/lib/libvkbasalt.so"
rm -f "$fc_data_dir/framecolor.cube" "$fc_data_dir/vkBasalt.conf"
rm -f "$fc_data_dir/SUPPORTED_STEAMVR_VERSION"
rm -f "$fc_config_dir/current.txt" "$fc_config_dir/presets.txt" "$fc_config_dir/vulkan.env" "$fc_config_dir/guard-failure-limit" "$fc_config_dir/language.txt"

rmdir "$fc_data_dir/layer" "$fc_data_dir/lib" "$fc_data_dir" 2>/dev/null || true
rmdir "$fc_config_dir" "$fc_unit_dir/steamvr.service.d" 2>/dev/null || true
systemctl --user daemon-reload
if [ "$fc_was_enabled" = true ]; then
    systemctl --user restart steamvr.service
fi
rmdir "$fc_config_dir" 2>/dev/null || true
printf 'FrameColor files and saved settings were removed. SteamVR settings changed by the dashboard may remain.\n'
