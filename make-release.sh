#!/usr/bin/env bash
# Create a release tarball.
#
# This is different from git archive in that it includes some untracked files.
# We would like to package all of the subprojects as fallbacks, so that they can
# be used without downloading anything.
#
# usage:
# ./make-release.sh
#     make release from HEAD, using "git describe" for the version
#
# ./make-release.sh tag
#     make release from tag

set -euo pipefail

RELEASE=${1:-$(git describe --tags)}
RELNOV=${RELEASE#v}
WORK=$(pwd)

git worktree add /tmp/cbot-$RELEASE $RELEASE
pushd /tmp/cbot-$RELEASE

meson subprojects download
meson subprojects foreach rm -rf .git

tar --exclude-vcs --transform "s,^\.,cbot-$RELNOV," -czvf $WORK/cbot-$RELNOV.tar.gz .
popd
git worktree remove /tmp/cbot-$RELEASE
