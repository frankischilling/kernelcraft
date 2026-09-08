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
if "$fixture/game/test-startup" >missing.log 2>&1; then
  echo 'Missing shaders must fail startup' >&2
  exit 1
fi
grep -q 'Failed to open shader file' missing.log
cp "$assets/shaders/vertex_shader.glsl" "$fixture/game/assets/shaders/"
rm "$fixture/game/assets/textures/dirt.png"
if "$fixture/game/test-startup" >missing.log 2>&1; then
  echo 'Missing textures must fail startup' >&2
  exit 1
fi
grep -q 'Failed to load texture' missing.log
echo 'Application startup, missing assets, and shutdown tests passed'
