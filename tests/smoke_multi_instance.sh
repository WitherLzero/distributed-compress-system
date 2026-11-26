# 票 07 CP7.1：Master-Server 多实例烟测（设计 §6.4：强杀实例是常态路径）。
# 用法: smoke_multi_instance.sh [--build-dir DIR] [--n N]
#   SMOKE_FAST=1 用快进参数（心跳1s/ttl2s/扫描1s，ctest 与日常迭代）；
#   默认标准参数（心跳5s/ttl20s/扫描1s，验收口径）。
# 退出码 0=全断言通过；非 0=失败（trap 兜底清理，重复执行无残留）。
set -euo pipefail

BUILD_DIR=""
N=3
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --n) N="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done
if [[ -z "$BUILD_DIR" ]]; then
  BUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/build"
fi
MASTER_BIN="$BUILD_DIR/src/master/compress_master"
SERVER_BIN="$BUILD_DIR/src/server/compress_server"
QUERY="$BUILD_DIR/tests/smoke_query"

if [[ ! -x "$QUERY" || ! -x "$MASTER_BIN" || ! -x "$SERVER_BIN" ]]; then
  echo "binaries not found under $BUILD_DIR (need src/master/compress_master src/server/compress_server tests/smoke_query)" >&2
  exit 2
fi

# ---- 参数：SMOKE_FAST=1 快进（判死几秒可见）；默认标准（验收） ----
if [[ "${SMOKE_FAST:-0}" == "1" ]]; then
  HB_INTERVAL=1; DEATH_TTL=2; DEATH_SCAN=1
else
  HB_INTERVAL=5; DEATH_TTL=20; DEATH_SCAN=1
fi

WORK="$(mktemp -d /tmp/smoke.XXXXXX)"
PIDFILE="$WORK/pids"          # 每行 "<pid> <role>"——$! 直取，禁止 pgrep -f（探针伪影教训）
MASTER_LOG="$WORK/master.log"
touch "$PIDFILE"

# trap 兜底：任何退出路径（含失败 set -e）都强杀登记进程并清场
cleanup() {
  while read -r pid role; do
    [[ -z "${pid:-}" ]] && continue
    if kill -0 "$pid" 2>/dev/null; then
      kill -9 "$pid" 2>/dev/null || true
    fi
  done < "$PIDFILE"
  rm -rf "$WORK"
}
trap cleanup EXIT

# 随机高端口，避开固定端口在重复执行/并行场景撞车
pick_free_port() {
  local port
  while :; do
    port=$(( (RANDOM % 20000) + 30000 ))
    if ! ss -ltn | grep -q ":$port "; then
      echo "$port"; return
    fi
  done
}
MASTER_PORT="$(pick_free_port)"
MASTER_ADDR="127.0.0.1:$MASTER_PORT"

say() { echo "== $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

# 轮询断言：查询直到满足条件或超时（不固定 sleep——慢机上固定 sleep 必 flaky）
wait_count() {  # wait_count <期望数> <超时秒> <提示>
  local expect="$1" deadline="$2" hint="$3"
  local end=$((SECONDS + deadline))
  while (( SECONDS < end )); do
    if out="$("$QUERY" "$MASTER_ADDR" "$expect" 2>&1)"; then
      echo "$out"
      return 0
    fi
    sleep 0.2
  done
  echo "$out" || true
  fail "$hint (expect=$expect, ${SECONDS}s)"
}

say "start master ($MASTER_ADDR, hb=${HB_INTERVAL}s ttl=${DEATH_TTL}s scan=${DEATH_SCAN}s)"
"$MASTER_BIN" --port "$MASTER_PORT" --heartbeat_interval_sec "$HB_INTERVAL" \
  --death_ttl_sec "$DEATH_TTL" --death_scan_interval_sec "$DEATH_SCAN" \
  >"$MASTER_LOG" 2>&1 &
echo "$! master" >> "$PIDFILE"

# master 就绪 = 查询 RPC 可达（空表也算就绪）
wait_count 0 15 "master not ready"

say "start $N servers"
for i in $(seq 1 "$N"); do
  "$SERVER_BIN" --port 0 --master_addr "$MASTER_ADDR" \
    >>"$WORK/server_$i.log" 2>&1 &
  echo "$! server#$i" >> "$PIDFILE"
done

wait_count "$N" 30 "servers did not all register"

say "kill -9 server#2 (scheduler reclaim; pid from pidfile)"
TARGET="$(awk '$2=="server#2" {print $1}' "$PIDFILE")"
[[ -n "$TARGET" ]] || fail "server#2 pid not in pidfile"
kill -9 "$TARGET"
say "killed pid=$TARGET; entry lingers until ttl=${DEATH_TTL}s (判死非即时)"

# 收缩上限 = ttl + 2×扫描周期 + 余量（快进 5s / 标准 24s）
BOUND=$(( DEATH_TTL + 2 * DEATH_SCAN + 2 ))
wait_count $((N - 1)) "$BOUND" "registry did not shrink after kill"

say "graceful stop: TERM master + remaining servers"
while read -r pid role; do
  kill -TERM "$pid" 2>/dev/null || true
done < "$PIDFILE"
RC_OK=1
while read -r pid role; do
  if ! timeout 15 tail --pid="$pid" -f /dev/null 2>/dev/null; then
    echo "WARN: $role pid=$pid did not exit within 15s after TERM" >&2
    RC_OK=0
  fi
done < "$PIDFILE"
[[ "$RC_OK" == "1" ]] || fail "graceful stop incomplete"

say "PASS: N=$N registered -> kill -> shrink to $((N-1)) within ${BOUND}s -> graceful stop"
