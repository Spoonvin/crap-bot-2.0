#!/usr/bin/env bash
set -euo pipefail

base_flags="$CXXFLAGS"
profile_dir=$(mktemp -d /tmp/crap-bot-pgo.XXXXXX)

make clean
make USE_GUI=0 \
    CXXFLAGS="$base_flags -fprofile-generate=$profile_dir -fprofile-update=atomic"

while IFS= read -r fen || [ -n "$fen" ]; do
  [ -z "$fen" ] && continue
  ./app test_fen "$fen" 3000
done < pgo-positions.txt

make clean
make USE_GUI=0 \
  CXXFLAGS="$base_flags -fprofile-use=$profile_dir"