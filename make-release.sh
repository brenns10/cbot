#!/usr/bin/env bash
# Create a release tarball.
#
# This is different from git archive in that it includes some untracked files.
# We would like to package all of the subprojects as fallbacks, so that they can
# be used without downloading anything.
set -euo pipefail

RELEASE=v0.4.0
WORK=$(pwd)

git worktree add /tmp/cbot-$RELEASE HEAD
pushd /tmp/cbot-$RELEASE

meson subprojects download
meson subprojects foreach rm -rf .git

tar --exclude-vcs --transform "s,^\.,cbot-$RELEASE," -czvf $WORK/cbot-$RELEASE.tar.gz .
popd
git worktree remove /tmp/cbot-$RELEASE
