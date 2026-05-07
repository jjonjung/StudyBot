#!/bin/bash
# StudyBot 서버 빌드 & 실행 스크립트 (WSL용)
# 실행: bash /mnt/c/Users/EJ/Desktop/Fork/StudyBot/build.sh

set -e

SRC="/mnt/c/Users/EJ/Desktop/Fork/StudyBot/server"
DST="$HOME/studybot_server"

echo "=== [1/3] 소스 동기화 ==="
rsync -a --delete "$SRC/" "$DST/"

echo "=== [2/3] CMake 빌드 ==="
mkdir -p "$DST/build"
cd "$DST/build"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

echo "=== [3/3] 서버 재시작 ==="
pkill -f studybot_server 2>/dev/null && sleep 1 || true
nohup "$DST/build/studybot_server" > "$DST/server.log" 2>&1 &
sleep 1

echo ""
echo "=== 동작 확인 ==="
curl -s http://localhost:3000/health | python3 -m json.tool 2>/dev/null || curl http://localhost:3000/health
echo ""
echo "PID: $(pgrep -f studybot_server)"
echo "Log: $DST/server.log"
