#!/bin/sh
iwyu-tool -p build "$@" | (cd build && iwyu-fix-includes --nocomments --only_re "$(pwd).*")
