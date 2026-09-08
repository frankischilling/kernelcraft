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
"$fixture/game/test-startup"
rm "$fixture/game/assets/shaders/vertex_shader.glsl"
status=0
"$fixture/game/test-startup" >missing.log 2>&1 || status=$?
test "$status" -eq 1 || { echo "Missing shader exit: $status (expected 1)" >&2; exit 1; }
grep -q 'Failed to open shader file' missing.log
if grep -q 'Application smoke test:' missing.log; then cat missing.log; exit 1; fi
cp "$assets/shaders/vertex_shader.glsl" "$fixture/game/assets/shaders/"
rm "$fixture/game/assets/textures/dirt.png"
status=0
"$fixture/game/test-startup" >missing.log 2>&1 || status=$?
test "$status" -eq 1 || { echo "Missing texture exit: $status (expected 1)" >&2; exit 1; }
grep -q 'Failed to load texture' missing.log
if grep -q 'Application smoke test:' missing.log; then cat missing.log; exit 1; fi
echo 'Application startup, missing assets, and shutdown tests passed'
