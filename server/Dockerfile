# builds the emu servers on linux so tests avoid the windows antivirus build context is the repo root
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config \
        libmariadb-dev libssl-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY shared ./shared
COPY server ./server

RUN cmake -S server -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target LoginServer GameServer

FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libmariadb3 libssl3 ca-certificates netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/bin/LoginServer /src/build/bin/GameServer /app/
COPY server/dist/config /app/config
COPY server/scripts /app/scripts
RUN mkdir -p /app/logs

# ini files default to localhost containers get compose service names patched in at boot
COPY <<'EOF' /usr/local/bin/knc-entrypoint.sh
#!/bin/sh
set -e

# rewrites key equals value only inside the named section since ini files share host keys across sections
patch_ini() {
    f="$1"
    pairs="$2"
    [ -f "$f" ] || { echo "missing config $f" >&2; exit 1; }
    awk -v pairs="$pairs" '
        BEGIN {
            n = split(pairs, arr, "\n")
            for (i = 1; i <= n; i++) {
                if (arr[i] == "") continue
                eq = index(arr[i], "=")
                want[substr(arr[i], 1, eq - 1)] = substr(arr[i], eq + 1)
            }
        }
        /^[ \t]*\[/ {
            sec = $0
            sub(/^[ \t]*\[/, "", sec)
            sub(/\].*$/, "", sec)
            print
            next
        }
        {
            if ($0 ~ /^[ \t]*[A-Za-z_][A-Za-z0-9_]*[ \t]*=/) {
                key = $0
                sub(/[ \t]*=.*$/, "", key)
                gsub(/^[ \t]+|[ \t]+$/, "", key)
                full = sec "." key
                if (full in want) { print key " = " want[full]; next }
            }
            print
        }
    ' "$f" > "$f.tmp" && mv "$f.tmp" "$f"
}

wait_for() {
    host="$1"
    port="$2"
    label="$3"
    i=0
    until nc -z "$host" "$port" 2>/dev/null; do
        i=$((i + 1))
        if [ "$i" -ge 120 ]; then
            echo "timeout waiting for $label at $host:$port" >&2
            exit 1
        fi
        sleep 1
    done
    echo "[entrypoint] $label up at $host:$port"
}

DB_HOST="${DB_HOST:-mariadb}"
DB_PORT="${DB_PORT:-3306}"
DB_NAME="${DB_NAME:-knc_emu}"
DB_USER="${DB_USER:-knc}"
DB_PASSWORD="${DB_PASSWORD:-knc_password}"
LOGIN_HOST="${LOGIN_HOST:-login-server}"
LOGIN_PORT="${LOGIN_PORT:-50017}"
GAME_PORT="${GAME_PORT:-50018}"
# address handed to the client in the server list  client runs on the docker host
GAME_PUBLIC_HOST="${GAME_PUBLIC_HOST:-127.0.0.1}"

DB_PAIRS="Database.host=$DB_HOST
Database.port=$DB_PORT
Database.name=$DB_NAME
Database.user=$DB_USER
Database.password=$DB_PASSWORD"

mkdir -p /app/logs

# passive listen probe  a real connect would spam the server log every few sec
# a listening port only proves the kernel holds the socket  the third argument is the
# liveness stamp the accept loop rewrites  a frozen loop stops touching it and the
# container turns unhealthy instead of staying healthy while it is deaf
if [ "$1" = "healthcheck" ]; then
    hex=$(printf '%04X' "$2")
    awk -v hex="$hex" '
        $4 == "0A" && substr($2, length($2) - 3) == hex { found = 1 }
        END { exit !found }
    ' /proc/net/tcp /proc/net/tcp6 || exit 1

    stamp="${3:-}"
    [ -n "$stamp" ] || exit 0
    [ -f "$stamp" ] || exit 1
    age=$(( $(date +%s) - $(stat -c %Y "$stamp") ))
    [ "$age" -le "${KNC_LIVENESS_MAX_AGE:-30}" ] || {
        echo "accept loop stamp is $age s old the server takes no new client" >&2
        exit 1
    }
    exit 0
fi

case "$1" in
login)
    patch_ini /app/config/loginserver.ini "$DB_PAIRS
Server.port=$LOGIN_PORT"
    wait_for "$DB_HOST" "$DB_PORT" mariadb
    exec /app/LoginServer --config config/loginserver.ini
    ;;
game)
    patch_ini /app/config/gameserver.ini "$DB_PAIRS
Server.host=$GAME_PUBLIC_HOST
Server.port=$GAME_PORT
LoginServer.host=$LOGIN_HOST
LoginServer.port=$LOGIN_PORT"
    wait_for "$DB_HOST" "$DB_PORT" mariadb
    wait_for "$LOGIN_HOST" "$LOGIN_PORT" login-server
    # the accept loop rewrites this file every five seconds the healthcheck reads its age
    KNC_LIVENESS_FILE="${KNC_LIVENESS_FILE:-/app/logs/gameserver.alive}"
    export KNC_LIVENESS_FILE
    rm -f "$KNC_LIVENESS_FILE"
    exec /app/GameServer --config config/gameserver.ini
    ;;
*)
    exec "$@"
    ;;
esac
EOF
RUN chmod +x /usr/local/bin/knc-entrypoint.sh

# login 50017  game 50018  internal registration 50100
EXPOSE 50017 50018 50100
ENTRYPOINT ["/usr/local/bin/knc-entrypoint.sh"]
CMD ["login"]
