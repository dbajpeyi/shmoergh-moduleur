#!/usr/bin/env bash
# Prints the text of a GitHub release: the version's section of CHANGELOG.md,
# then the install notes. For a release that is already tagged, the install
# notes come from that tag, so older releases keep the steps that fit them.
#
#   .github/scripts/release-notes.sh v1.2.3
set -euo pipefail

tag="${1:?usage: release-notes.sh vX.Y.Z}"
root="$(git rev-parse --show-toplevel)"

# Lines after "## <tag> ..." up to the next "## " heading, without leading or
# trailing blank lines.
changes="$(awk -v tag="$tag" '
    $1 == "##" { if (found) exit; found = ($2 == tag); next }
    found' "$root/CHANGELOG.md" | sed -e '/./,$!d')"

if [ -z "$changes" ]; then
    echo "CHANGELOG.md has no section for $tag. Add a \"## $tag — <date>\" heading with the changes." >&2
    exit 1
fi

printf '%s\n\n---\n\n' "$changes"
if git -C "$root" rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
    git -C "$root" show "$tag:.github/release-notes.md"
else
    cat "$root/.github/release-notes.md"
fi
