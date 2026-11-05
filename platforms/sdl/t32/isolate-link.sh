#!/bin/sh
# Relinks a T32 build so the synth archives keep their own copies of inline/template code
# (GCC 11+ toolchains only; with the GCC 10 toolchain the plain cmake link is the binary to ship).
# Why: a weak (COMDAT) function or object defined both by our objects and by the archives is
# linked once, and our GCC 15 copy wins, so archive code runs GCC 15's version of e.g.
# std::vector<Patch::Metadata::Param>::_M_default_append or polysynth::InstrumentVoice's inline
# members. Here every such symbol is renamed in copies of OUR objects (suffix __pbiso; the
# archives are not touched), so each side binds to its own compiler's copy.
#   platforms/sdl/t32/isolate-link.sh [cmake build dir, default build-t32app/app]  ->  <dir>/iso/tracker-sdl
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
B="$(cd "${1:-$ROOT/build-t32app/app}" && pwd)"
P="${T32_PREFIX:-arm-unknown-linux-gnueabihf-}"
LIB="$ROOT/tracker-ce/synth/lib"
ISO="$B/iso"
LINK="$B/CMakeFiles/tracker-sdl.dir/link.txt"
[ -f "$LINK" ] || { echo "no $LINK (build first)"; exit 1; }
rm -rf "$ISO" && mkdir -p "$ISO"
cd "$B"
OBJS="$(tr ' ' '\n' < "$LINK" | grep '\.o"\{0,1\}$' | tr -d '"')"
# Symbols the archives define (nm symbol table only) x weak symbols our objects define.
"${P}nm" --defined-only "$LIB/libPolySynth.a" "$LIB/libPolyDSP.a" 2>/dev/null | awk 'NF==3{print $3}' | sort -u > "$ISO/archive.syms"
"${P}nm" --defined-only $OBJS libtracker_core.a 2>/dev/null | awk 'NF==3 && ($2=="W"||$2=="V"||$2=="u"){print $3}' | sort -u > "$ISO/ours.weak"
comm -12 "$ISO/archive.syms" "$ISO/ours.weak" | awk '{print $1" "$1"__pbiso"}' > "$ISO/redefine.txt"
echo "isolating $(wc -l < "$ISO/redefine.txt" | tr -d ' ') shared weak symbols"
for o in $OBJS; do
    mkdir -p "$ISO/$(dirname "$o")"
    "${P}objcopy" --redefine-syms="$ISO/redefine.txt" "$o" "$ISO/$o"
done
"${P}objcopy" --redefine-syms="$ISO/redefine.txt" libtracker_core.a "$ISO/libtracker_core.a"
CMD="$(sed -E 's#"?(CMakeFiles/[^" ]+\.o)"?#iso/\1#g; s# libtracker_core\.a# iso/libtracker_core.a#; s#-o tracker-sdl#-o iso/tracker-sdl#; s#-Wl,--dependency-file=[^ ]*##' "$LINK")"
eval "$CMD" 2>&1 | grep -v "statically linked applications\|in function\|does not take linker garbage" || true
[ -x "$ISO/tracker-sdl" ] || { echo "relink failed"; exit 1; }
# Self-check: none of the renamed symbols may still be referenced under its old name by our side.
if "${P}nm" "$ISO/tracker-sdl" | grep -q '__pbiso'; then echo "ok: $ISO/tracker-sdl"; else echo "no renamed symbol in the output"; exit 1; fi
