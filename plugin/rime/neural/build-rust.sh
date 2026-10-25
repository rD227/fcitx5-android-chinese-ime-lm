#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
cd "$(dirname "$0")"
export PATH="$HOME/.cargo/bin:$PATH"
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$HOME/.cache/fcitx-neural-target}"
if [ "${1:-}" = "--test" ]; then
    cargo test --manifest-path vendor/chinese-ime-lm/Cargo.toml --locked
fi
cargo build --release --target aarch64-linux-android --locked
mkdir -p target/aarch64-linux-android/release
cp "$CARGO_TARGET_DIR/aarch64-linux-android/release/librime_chinese_ime_lm.a" \
    target/aarch64-linux-android/release/
