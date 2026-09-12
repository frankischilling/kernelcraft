#!/bin/sh
# A real application entry point, with a hidden window and bounded frame count.
set -eu
binary=$(realpath "$1")
assets=$(dirname "$binary")/assets
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT HUP INT TERM
mkdir "$fixture/game" "$fixture/working"
cp "$binary" "$fixture/game/test-startup"
cp -R "$assets" "$fixture/game/assets"
cd "$fixture/working"
printf 'existing default save sentinel' >kernelcraft.kcw
cp kernelcraft.kcw original.kcw
"$fixture/game/test-startup" --no-save
cmp kernelcraft.kcw original.kcw
rm "$fixture/game/assets/shaders/vertex_shader.glsl"
status=0
"$fixture/game/test-startup" --no-save >missing.log 2>&1 || status=$?
test "$status" -eq 1 || { echo "Missing shader exit: $status (expected 1)" >&2; exit 1; }
grep -q 'Failed to open shader file' missing.log
if grep -q 'Application smoke test:' missing.log; then cat missing.log; exit 1; fi
cp "$assets/shaders/vertex_shader.glsl" "$fixture/game/assets/shaders/"
rm "$fixture/game/assets/textures/dirt.png"
status=0
"$fixture/game/test-startup" --no-save >missing.log 2>&1 || status=$?
test "$status" -eq 1 || { echo "Missing texture exit: $status (expected 1)" >&2; exit 1; }
grep -q 'Failed to load texture' missing.log
if grep -q 'Application smoke test:' missing.log; then cat missing.log; exit 1; fi
cp "$assets/textures/dirt.png" "$fixture/game/assets/textures/"
for moon in full-moon waning-gibbous last-quarter waning-crescent new-moon waxing-crescent first-quarter waxing-gibbous; do
  rm "$fixture/game/assets/sky/$moon.png"
  status=0
  "$fixture/game/test-startup" --no-save >missing.log 2>&1 || status=$?
  test "$status" -eq 1 || { echo "Missing moon exit: $status (expected 1)" >&2; exit 1; }
  grep -q 'Failed to initialize sky rendering' missing.log
  if grep -q 'Application smoke test:' missing.log; then cat missing.log; exit 1; fi
  cp "$assets/sky/$moon.png" "$fixture/game/assets/sky/"
done
echo 'Application startup, missing assets, and shutdown tests passed'
