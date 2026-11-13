#!/bin/sh
# Builds game.wasm and the single-file playable page (needs wasi-sdk; set SDK).
SDK=${SDK:-/tmp/claude-0/-home-claude/c3935d97-3cec-55d2-945b-c7c967b40c34/scratchpad/wasi/wasi-sdk-24.0-x86_64-linux}
G=../src/game
cd "$(dirname "$0")"
$SDK/bin/clang++ --sysroot=$SDK/share/wasi-sysroot --target=wasm32-wasi -std=c++17 -Os -fno-exceptions -fno-rtti -mexec-model=reactor -Wl,--no-entry -Wl,--gc-sections -Wl,--strip-all -w -o game.wasm web_main.cpp $G/canvas.cpp $G/catart.cpp $G/catsprites.cpp $G/catsprites_data.cpp $G/chibi.cpp $G/livewindow.cpp $G/extras.cpp $G/explore.cpp $G/roomdata.cpp $G/gfx.cpp $G/compose.cpp $G/activities.cpp $G/breeds.cpp $G/pet.cpp $G/save.cpp $G/platform.cpp $G/motion.cpp $G/game.cpp $G/home.cpp $G/screens.cpp $G/minigames.cpp $G/netplay.cpp || exit 1
python3 - <<'PY'
import base64
t=open('template.html').read()
w=base64.b64encode(open('game.wasm','rb').read()).decode()
open('kitten-keeper-play.html','w').write(t.replace('__WASM__',w))
PY
ls -la game.wasm kitten-keeper-play.html
rm -f game.wasm
