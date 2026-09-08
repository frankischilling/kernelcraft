#!/bin/sh
# Run real builds in a temporary source copy; never touch the working sources.
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT HUP INT TERM
cp "$project/Makefile" "$fixture/"
cp -R "$project/src" "$project/libs" "$project/tests" "$fixture/"
cd "$fixture"
make test
object=obj/linux/Release/src/world/world.o
test -f "$object" || { echo 'Release must have separate object output' >&2; exit 1; }
cp -p "$object" original.o
make test
test ! "$object" -nt original.o || { echo 'Unchanged builds must reuse objects' >&2; exit 1; }
sleep 1
touch src/world/chunk.h
make test
test "$object" -nt original.o || { echo 'Header changes must rebuild callers' >&2; exit 1; }
cp -p "$object" original.o
sleep 1
make CFLAGS='-O0 -g3' test
test "$object" -nt original.o || { echo 'CFLAGS changes must rebuild objects' >&2; exit 1; }
cp -p "$object" original.o
make CONFIGURATION=Debug test
test -f obj/linux/Debug/src/world/world.o
test -x bin/linux/Debug/test-world
test ! "$object" -nt original.o || { echo 'Debug must preserve Release objects' >&2; exit 1; }
sleep 1
CC=clang make test
test "$object" -nt original.o || { echo 'Environment compiler override must rebuild objects' >&2; exit 1; }
echo 'Build regression tests passed'
