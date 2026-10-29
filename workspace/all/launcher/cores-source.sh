#!/bin/sh
# Prints where the source of each emulator core on a Bramley card comes from: the upstream repo,
# the exact commit, and the patches Bramley applies. It goes on the card as CORES-SOURCE.md, so a
# release names the exact source of the GPL cores it ships (docs/plans/cores-licenses.md).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
MK="$ROOT/workspace/tg5040/cores/makefile"

echo "# Source of the emulator cores"
echo
echo "Each core is built from the upstream commit below, with the patches in the Bramley repo"
echo "(github.com/howardhwt/bramley) applied, by workspace/tg5040/cores/makefile. The licenses are"
echo "listed in docs/plans/cores-licenses.md in that repo; the source of each Bramley release is its"
echo "tag there."
echo
echo "| Core | Upstream | Commit | Patches |"
echo "|---|---|---|---|"
for core in $(grep '^CORES' "$MK" | awk '{print $2}'); do
	repo=$(grep "^${core}_REPO *=" "$MK" | head -1 | sed 's/.*= *//')
	[ -n "$repo" ] || repo="https://github.com/libretro/$core"
	hash=$(grep "^${core}_HASH *=" "$MK" | tail -1 | sed 's/.*= *//')
	[ -n "$hash" ] || { echo "$core is not pinned" >&2; exit 1; }
	patches=""
	[ -f "$ROOT/workspace/tg5040/cores/patches/$core.patch" ] && patches="workspace/tg5040/cores/patches/$core.patch"
	for p in "$ROOT/workspace/all/cores/patches/$core/"*.patch; do
		[ -f "$p" ] && patches="$patches ${p#$ROOT/}"
	done
	echo "| $core | ${repo%.git} | $hash | ${patches:-none} |"
done
