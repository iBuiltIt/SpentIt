#!/bin/sh
# Per-user SpentIt installer. tools/package.py writes the payload offset and
# appends the archive. Run it with: sh SpentIt-setup.run
set -eu

launch=1
if [ "${1:-}" = "--no-launch" ]; then
  launch=0
fi

if [ -z "${HOME:-}" ]; then
  echo "SpentIt could not find your home folder." >&2
  exit 1
fi

# Lock files already use ~/.local/share/SpentIt. The program files stay beside that.
prefix="$HOME/.local/opt/SpentIt"
apps="$HOME/.local/share/applications"
mime_root="$HOME/.local/share/mime"
icons="$HOME/.local/share/icons/hicolor"
desktop="$apps/SpentIt.desktop"
raw_offset=@PAYLOAD_OFFSET@
# The stored offset is zero-padded. Dash treats a leading zero as octal.
offset=$(printf '%s\n' "$raw_offset" | sed 's/^0*//')
if [ -z "$offset" ]; then
  offset=0
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
tail -c +$((offset + 1)) "$0" > "$work/payload.tar.gz"
tar -xzf "$work/payload.tar.gz" -C "$work"
if [ ! -x "$work/SpentIt/SpentIt" ]; then
  echo "This SpentIt installer is incomplete." >&2
  exit 1
fi

mkdir -p "$(dirname "$prefix")"
if [ -d "$prefix" ] && ! rm -rf "$prefix"; then
  echo "Quit SpentIt, then run this installer again." >&2
  exit 1
fi
mkdir -p "$prefix" "$apps" "$mime_root/packages"

cp -a "$work/SpentIt/." "$prefix/"
if [ -d "$prefix/share/icons/hicolor" ]; then
  mkdir -p "$icons"
  cp -a "$prefix/share/icons/hicolor/." "$icons/"
fi

cat > "$desktop" << EOF
[Desktop Entry]
Type=Application
Name=SpentIt
Comment=Where did my money go?
Exec=$prefix/SpentIt %f
Icon=$prefix/share/icons/hicolor/256x256/apps/SpentIt.png
Terminal=false
Categories=Office;Finance;
MimeType=application/x-spentit;
StartupWMClass=SpentIt
EOF

cat > "$mime_root/packages/spentit.xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-spentit">
    <comment>SpentIt file</comment>
    <icon name="SpentIt"/>
    <glob pattern="*.spentit"/>
  </mime-type>
</mime-info>
EOF

cat > "$prefix/uninstall" << EOF
#!/bin/sh
set -eu
if [ -d "$prefix" ] && ! rm -rf "$prefix"; then
  echo "Quit SpentIt, then run uninstall again." >&2
  exit 1
fi
rm -f "$desktop"
rm -f "$mime_root/packages/spentit.xml"
for size in 16 32 64 128 256 512 1024; do
  rm -f "$icons/\${size}x\${size}/apps/SpentIt.png"
done
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$apps" >/dev/null 2>&1 || true
fi
if command -v update-mime-database >/dev/null 2>&1; then
  update-mime-database "$mime_root" >/dev/null 2>&1 || true
fi
echo "SpentIt was removed. Files you saved were not removed."
EOF
chmod 755 "$prefix/uninstall" "$prefix/SpentIt" "$prefix/SpentIt.bin"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$apps" >/dev/null 2>&1 || true
fi
if command -v update-mime-database >/dev/null 2>&1; then
  update-mime-database "$mime_root" >/dev/null 2>&1 || true
fi
if command -v xdg-mime >/dev/null 2>&1; then
  xdg-mime default SpentIt.desktop application/x-spentit >/dev/null 2>&1 || true
fi

echo "SpentIt is installed."
if [ "$launch" -eq 1 ] && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
  if command -v nohup >/dev/null 2>&1; then
    nohup "$prefix/SpentIt" >/dev/null 2>&1 &
  else
    "$prefix/SpentIt" >/dev/null 2>&1 &
  fi
fi
exit 0
