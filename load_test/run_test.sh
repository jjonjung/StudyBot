#!/bin/bash
# StudyBot 부하 테스트 실행 스크립트
# 사전 조건: pip install locust

set -e

HOST="${HOST:-http://localhost:3000}"
USERS="${USERS:-50}"
SPAWN_RATE="${SPAWN_RATE:-10}"
DURATION="${DURATION:-60s}"
OUT_DIR="$(dirname "$0")/results"

mkdir -p "$OUT_DIR"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "=== StudyBot 부하 테스트 시작 ==="
echo "  Host       : $HOST"
echo "  Users      : $USERS"
echo "  Spawn rate : $SPAWN_RATE/s"
echo "  Duration   : $DURATION"
echo "  Output     : $OUT_DIR/result_${TIMESTAMP}"
echo ""

locust -f "$(dirname "$0")/locustfile.py" \
    --host="$HOST" \
    --headless \
    -u "$USERS" \
    -r "$SPAWN_RATE" \
    --run-time "$DURATION" \
    --csv="$OUT_DIR/result_${TIMESTAMP}" \
    --html="$OUT_DIR/report_${TIMESTAMP}.html" \
    --logfile="$OUT_DIR/locust_${TIMESTAMP}.log"

echo ""
echo "=== 테스트 완료 ==="
echo "  CSV  : $OUT_DIR/result_${TIMESTAMP}_stats.csv"
echo "  HTML : $OUT_DIR/report_${TIMESTAMP}.html"
