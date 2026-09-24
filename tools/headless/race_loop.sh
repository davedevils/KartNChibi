#!/usr/bin/env bash
# accounts default to hltest and hltest2 which already hold plenty of gold and cash

set -u
HOST=127.0.0.1
WINNER=hltest
WINPASS=test
CHASER=hltest2
CHASEPASS=test
PORT=50017
ROUNDS=20
OUT="${TMPDIR:-/tmp}/knc_loop"
BIN="$(cd "$(dirname "$0")/../../build-headless/Release" && pwd)"

while [ $# -gt 0 ]; do
  case "$1" in
    --host)   HOST="$2"; shift 2 ;;
    --port)   PORT="$2"; shift 2 ;;
    --rounds) ROUNDS="$2"; shift 2 ;;
    --out)    OUT="$2"; shift 2 ;;
    --winner) WINNER="$2"; WINPASS="$2"; shift 2 ;;
    --chaser) CHASER="$2"; CHASEPASS="$2"; shift 2 ;;
    *) echo "unknown option $1"; exit 1 ;;
  esac
done
mkdir -p "$OUT"

# every track id that resolves to a shipped racing line
TRACKS=(10 11 12 13 20 21 22 23 30 31 32 33 40 41 42 43 50 51 52 53 56
        60 61 62 63 70 71 72 73 74 80 81 82 90 91)

kill_bots() {
  for pid in $(tasklist 2>/dev/null | grep -i "KnC-Headless" | awk '{print $2}'); do
    taskkill //PID "$pid" //F >/dev/null 2>&1
  done
}

echo "race loop  $WINNER vs $CHASER  target $HOST:$PORT  $ROUNDS rounds  logs in $OUT"
kill_bots

for r in $(seq 1 "$ROUNDS"); do
  TRACK=${TRACKS[$((RANDOM % ${#TRACKS[@]}))]}
  # winner is slightly quicker so the gap stays within a second or two over three laps
  WIN_PACE=$(awk -v s=$RANDOM 'BEGIN{srand(s); printf "%.3f", 0.86 + rand()*0.06}')
  LOSE_PACE=$(awk -v s=$RANDOM 'BEGIN{srand(s); printf "%.3f", 0.90 + rand()*0.06}')

  echo
  echo "round $r/$ROUNDS  track $TRACK  $WINNER pace $WIN_PACE  $CHASER pace $LOSE_PACE"
  rm -f "$BIN/roomid.txt"

  ( cd "$BIN" && timeout 420 ./KnC-Headless.exe "$HOST" "$PORT" "$WINNER" "$WINPASS" \
      "$OUT/r${r}_host.log" follow.ini host "$TRACK" 0 "$WIN_PACE" \
      > "$OUT/r${r}_host.txt" 2>&1 ) &
  HOSTPID=$!

  # waits for the room id the host publishes then sends the chaser in
  for _ in $(seq 1 40); do
    [ -s "$BIN/roomid.txt" ] && break
    sleep 1
  done
  if [ ! -s "$BIN/roomid.txt" ]; then
    echo "  host never opened a room, skipping"
    kill_bots; continue
  fi
  echo "  room $(cat "$BIN/roomid.txt")"

  ( cd "$BIN" && timeout 400 ./KnC-Headless.exe "$HOST" "$PORT" "$CHASER" "$CHASEPASS" \
      "$OUT/r${r}_join.log" follow.ini join 0 0 "$LOSE_PACE" \
      > "$OUT/r${r}_join.txt" 2>&1 ) &

  wait $HOSTPID 2>/dev/null
  kill_bots

  grep -hoE "\[(TRACK|RACE|DRIFT|ITEM|HIT )\][^\"]*" "$OUT/r${r}_host.txt" 2>/dev/null | head -6
  for f in "$OUT/r${r}_host.log"; do
    [ -f "$f" ] || continue
    echo "  reward frames 0x3C: $(grep -c 'RX 0x003C' "$f" 2>/dev/null)" \
         " stats 0x0A: $(grep -c 'RX 0x000A' "$f" 2>/dev/null)"
  done
  sleep 8   # let the server settle the session before the next round
done

echo
echo "loop done, logs in $OUT"
