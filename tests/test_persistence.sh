#!/bin/sh
set -eu
binary=$(realpath "$1")
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT HUP INT TERM
cd "$fixture"
export KERNELCRAFT_TEST_WORLD="$fixture/world with spaces.kcw"
KERNELCRAFT_TEST_RESTART=save "$binary" --world 'world with spaces.kcw' --seed 42
test -f "$KERNELCRAFT_TEST_WORLD"
cp "$KERNELCRAFT_TEST_WORLD" original.kcw
KERNELCRAFT_TEST_RESTART=load "$binary" --world 'world with spaces.kcw'
cmp original.kcw "$KERNELCRAFT_TEST_WORLD"
KERNELCRAFT_TEST_WORLD="$fixture/crouched.kcw" KERNELCRAFT_TEST_RESTART=crouch-save "$binary" --world crouched.kcw --seed 42
cp crouched.kcw original-crouched.kcw
KERNELCRAFT_TEST_RESTART=crouch-load "$binary" --world crouched.kcw
cmp original-crouched.kcw crouched.kcw
status=0
KERNELCRAFT_TEST_WORLD="$fixture/missing/world.kcw" KERNELCRAFT_TEST_RESTART=fail "$binary" --world missing/world.kcw --seed 42 >error.log 2>&1 || status=$?
test "$status" -eq 1
grep -q 'Cannot save world' error.log
grep -q 'Application save failure status and cleanup checks passed' error.log
test ! -e missing
status=0
"$binary" --world 'world with spaces.kcw' --seed 7 >error.log 2>&1 || status=$?
test "$status" -eq 1
grep -q 'existing save' error.log
cmp original.kcw "$KERNELCRAFT_TEST_WORLD"
printf 'broken save' >corrupt.kcw
cp corrupt.kcw original-corrupt.kcw
status=0
"$binary" --world corrupt.kcw >error.log 2>&1 || status=$?
test "$status" -eq 1
grep -q 'Cannot load world' error.log
cmp corrupt.kcw original-corrupt.kcw
status=0
"$binary" --seed -1 >error.log 2>&1 || status=$?
test "$status" -eq 1
grep -q 'decimal integer' error.log
"$binary" --help >help.log
grep -q -- '--world' help.log
test ! -f kernelcraft.kcw
echo 'Process restart, launch-relative path, preserved saves, and CLI checks passed'
