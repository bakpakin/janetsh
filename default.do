#! /bin/sh
set -eu
exec 1>&2

target="$1"
out="$3"

. ./config.inc

# check we are in the right place.
test -d ./support/

shlib_csrc="$(echo shlib/*.c)"
shlib_obj="$(echo "$shlib_csrc" | sed 's/\.c/\.o/g')"

case $target in
  all)
    redo-ifchange shlib.so
    ;;
  clean)
    rm -f shlib.so $shlib_obj ./shlib/*.deps
    ;;
  install)
    redo-ifchange all
    mkdir -p "$PREFIX/bin/"
    mkdir -p "$PREFIX/lib/janetsh"
    install ./shlib.so "$PREFIX/lib/janetsh/"
    install ./sh.janet "$PREFIX/lib/janetsh/"
    head -n 1 ./janetsh > "$PREFIX/bin/janetsh"
    echo "(array/concat module/paths [" >> "$PREFIX/bin/janetsh"
    echo "  [\"$PREFIX/lib/janetsh/:all:.janet\" :source]" >> "$PREFIX/bin/janetsh"
    echo "  [\"$PREFIX/lib/janetsh/:all:.:native:\" :native]])" >> "$PREFIX/bin/janetsh"
    tail -n +2 ./janetsh >> "$PREFIX/bin/janetsh"
    chmod +x "$PREFIX/bin/janetsh"

    ;;
  shlib/*.o)
    cfile=shlib/$(basename $target .o).c
    redo-ifchange $cfile
    $CC -fPIC $JANET_HEADER_CFLAGS $CFLAGS -MMD -MF $cfile.deps -c -o $out $cfile
    redo-ifchange $cfile
    read DEPS < $cfile.deps
    redo-ifchange ${DEPS#*:}
    ;;
  shlib.so)
    redo-ifchange $shlib_obj
    $CC -shared $LDFLAGS $shlib_obj -o $out
    ;;
  *)
    echo "don't know how to build $target"
    exit 1 ;;
esac
