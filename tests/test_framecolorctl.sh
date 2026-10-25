#!/bin/sh
# Exercise the no-hardware disable/status path with a private fake home.
set -eu

fc_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fc_temp=$(mktemp -d)
fc_layer="$fc_temp/.local/share/framecolor/layer"
fc_log="$fc_temp/systemctl.log"
cleanup() {
    rm -f "$fc_layer/vkBasalt.json" "$fc_layer/vkBasalt.json.disabled" \
        "$fc_temp/bin/systemctl" "$fc_temp/bin/steamvr" "$fc_log" \
        "$fc_temp/.config/framecolor/guard-failure-limit" \
        "$fc_temp/.config/framecolor/language.txt"
    rmdir "$fc_layer" "$fc_temp/.local/share/framecolor" "$fc_temp/.local/share" \
        "$fc_temp/.local" "$fc_temp/.config/framecolor" "$fc_temp/.config" \
        "$fc_temp/bin" "$fc_temp" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM
mkdir -p "$fc_layer" "$fc_temp/bin" "$fc_temp/.config/framecolor"
printf 'test manifest\n' > "$fc_layer/vkBasalt.json"

printf '#!/bin/sh\nprintf "%%s\\n" "$*" >> "$FRAMECOLOR_TEST_LOG"\ncase "$*" in\n  "--user is-active framecolor.service") printf "inactive\\n" ;;\n  "--user is-enabled framecolor.service") printf "disabled\\n" ;;\n  "--user is-active steamvr.service") printf "active\\n" ;;\nesac\n' > "$fc_temp/bin/systemctl"
printf '#!/bin/sh\n[ "$1" = path ] && printf "/opt/steamvr\\n"\n' > "$fc_temp/bin/steamvr"
chmod +x "$fc_temp/bin/systemctl" "$fc_temp/bin/steamvr"

FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" disable
[ ! -f "$fc_layer/vkBasalt.json" ]
[ -f "$fc_layer/vkBasalt.json.disabled" ]
[ "$(sed -n '1p' "$fc_log")" = '--user disable --now framecolor.service' ]
[ "$(sed -n '2p' "$fc_log")" = '--user restart steamvr.service' ]

fc_status=$(FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" status)
printf '%s\n' "$fc_status" | grep -F 'Vulkan correction: disabled' >/dev/null
printf '%s\n' "$fc_status" | grep -F 'Dashboard service: inactive' >/dev/null
printf '%s\n' "$fc_status" | grep -F 'Dashboard autostart: disabled' >/dev/null
FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" guard 3
[ "$(cat "$fc_temp/.config/framecolor/guard-failure-limit")" = 3 ]
FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" guard 1
[ "$(cat "$fc_temp/.config/framecolor/guard-failure-limit")" = 1 ]

FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" language en
[ "$(cat "$fc_temp/.config/framecolor/language.txt")" = 'en' ]

fc_lang_status=$(FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" language status)
printf '%s\n' "$fc_lang_status" | grep -F 'Configured language: en' >/dev/null

FRAMECOLOR_TEST_LOG="$fc_log" HOME="$fc_temp" PATH="$fc_temp/bin:$PATH" \
    sh "$fc_root/contrib/framecolorctl" language ja
[ "$(cat "$fc_temp/.config/framecolor/language.txt")" = 'ja' ]

printf 'framecolorctl disable/status tests passed\n'
