#!/bin/bash
# boots the stock client under wine in xvfb then runs one scenario over the pilot bridge
# usage entrypoint sh boot or lobby or walk or race or refs or factory or paint or shell

set -u
SCENARIO="${1:-boot}"
KNC_HOST="${KNC_HOST:-host.docker.internal}"
KNC_USER="${KNC_USER:-hltest}"
KNC_PASS="${KNC_PASS:-hltestpw}"
KNC_BRIDGE_PORT="${KNC_BRIDGE_PORT:-47017}"
KNC_DESKTOP="${KNC_DESKTOP:-1280x1024}"
CLIENT_RO="${KNC_CLIENT_RO:-/client-ro}"
GAME=/game
RUN_ID="${KNC_RUN_ID:-$(date +%Y%m%d_%H%M%S)}_${SCENARIO}"
OUT="/out/${RUN_ID}"
mkdir -p "$OUT/logs"
export KNC_T0="$(date +%s.%N)"
export KNC_OUT="$OUT" KNC_GAME="$GAME" KNC_BRIDGE_PORT KNC_USER

log() { echo "$(date +%H:%M:%S) $*" | tee -a "$OUT/run.log"; }
since() { python3 -c "import time;print('%.1f' % (time.time() - $KNC_T0))"; }

# dinput8 is our pilot dll and d3d9 stays wine builtin since the folder ships a dgVoodoo copy
# msvcr71 is the shipped native one since the wine fscanf answers 0 not EOF and item init fails
export WINEDLLOVERRIDES="${KNC_DLLOVERRIDES:-dinput8=n,b;d3d9=b;msvcr71,msvcp71=n,b;mscoree,mshtml=;winemenubuilder.exe=d}"
export WINEDEBUG="${KNC_WINEDEBUG:-fixme-all}"

log "run $RUN_ID scenario $SCENARIO user $KNC_USER host $KNC_HOST"

Xvfb :99 -screen 0 "${KNC_DESKTOP}x24" -nolisten tcp -ac +extension GLX >"$OUT/logs/xvfb.log" 2>&1 &
for _ in $(seq 1 100); do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 0.1; done
log "xvfb up at $(since) s $(glxinfo -B 2>/dev/null | grep -m1 'renderer string' | sed 's/^ *//')"

if [ ! -f "$WINEPREFIX/system.reg" ]; then
    log "no baked prefix, creating one"
    wineboot -i >"$OUT/logs/wineboot.log" 2>&1
    wine regedit /S 'Z:\opt\knc\prefix.reg'
    wineserver -w
fi
wine reg add 'HKCU\Software\Wine\Explorer\Desktops' /v Default /d "$KNC_DESKTOP" /f >/dev/null 2>&1

# the client and the login redirect dial loopback so both ports are forwarded to the host
for port in 50017 50018; do
    socat "TCP-LISTEN:${port},bind=127.0.0.1,fork,reuseaddr" "TCP:${KNC_HOST}:${port}" >>"$OUT/logs/socat.log" 2>&1 &
done

if [ "$SCENARIO" = "shell" ]; then exec bash; fi

# files over 1 MB go to the cache volume once since reads over the desktop mount are slow
link_big() {
    local src="$1" name="$2" stamp
    stamp="$(stat -c '%s %Y' "$src")"
    if [ -d /cache ] && [ -w /cache ]; then
        if [ "$(cat "/cache/$name.stamp" 2>/dev/null)" != "$stamp" ]; then
            log "copying $name into the cache volume, once per change of the file"
            rm -f "/cache/$name.stamp"
            cp "$src" "/cache/$name.part" && mv -f "/cache/$name.part" "/cache/$name" && echo "$stamp" >"/cache/$name.stamp"
            log "cached $name at $(since) s"
        fi
        ln -s "/cache/$name" "$GAME/$name"
    else
        log "no cache volume so $name is read over the mount and the boot is slow"
        ln -s "$src" "$GAME/$name"
    fi
}

# the Data tree goes to the cache volume too keyed on a stamp the host runner computes
# with no stamp the tree is read over the mount which is correct but the lobby loads slowly
link_data() {
    local src="$1"
    local stamp="${KNC_DATA_STAMP:-}"
    if [ -n "$stamp" ] && [ -d /cache ] && [ -w /cache ]; then
        if [ "$(cat /cache/Data.stamp 2>/dev/null)" != "$stamp" ]; then
            log "copying Data into the cache volume, once per change of the tree"
            rm -rf /cache/Data.part /cache/Data.stamp
            mkdir -p /cache/Data.part
            (cd "$src" && tar cf - .) | (cd /cache/Data.part && tar xf -) \
                && rm -rf /cache/Data && mv /cache/Data.part /cache/Data && echo "$stamp" >/cache/Data.stamp
            log "cached Data at $(since) s"
        fi
        if [ -f /cache/Data.stamp ]; then
            ln -s /cache/Data "$GAME/Data"
            return
        fi
    fi
    log "Data read over the mount"
    ln -s "$src" "$GAME/Data"
}

# small files are real copies while Data and the big files are symlinks into the cache
if [ ! -f "$CLIENT_RO/KnC.exe" ]; then
    log "no client at $CLIENT_RO, mount DevClient there"
    exit 3
fi
mkdir -p "$GAME"
for f in "$CLIENT_RO"/*; do
    name="$(basename "$f")"
    case "$name" in
        extracted|ScreenShot|web|packets.log|clientpatch.log|launcher.log|*.orig) continue ;;
    esac
    if [ "$name" = "Data" ]; then
        link_data "$f"
    elif [ -d "$f" ]; then
        cp -r "$f" "$GAME/$name"
    elif [ "$(stat -c %s "$f")" -gt 1048576 ] && [ "${name##*.}" != "ini" ]; then
        link_big "$f" "$name"
    else
        cp "$f" "$GAME/$name"
    fi
done
mkdir -p "$GAME/ScreenShot"
log "game tree ready at $(since) s"

wine /opt/knc/pilot_bridge.exe "$KNC_BRIDGE_PORT" >"$OUT/logs/bridge.log" 2>&1 &

TOKEN="$(python3 /opt/knc/tools/launcher_login.py "$KNC_USER" "$KNC_PASS" 127.0.0.1 50017 2>>"$OUT/run.log")"
if [ -z "$TOKEN" ]; then
    log "launcher login refused or the server is not reachable on $KNC_HOST"
    exit 4
fi
log "token ok at $(since) s"

cd "$GAME" || exit 5
wine KnC.exe serviceid=1 "userid=$KNC_USER" "token=$TOKEN" >"$OUT/logs/wine.log" 2>&1 &
log "KnC exe started at $(since) s"

python3 /opt/knc/stock_docker/scenario.py "$SCENARIO" "$OUT"
rc=$?
log "scenario $SCENARIO exit $rc at $(since) s"
grep -m3 -E "Unhandled|page fault" "$OUT/logs/wine.log" | while read -r line; do log "client crash $line"; done
grep -a "\[MSG \] box" "$GAME/packets.log" 2>/dev/null | tail -3 | while read -r line; do log "client box $line"; done

for f in packets.log clientpatch.log Option2.ini; do
    [ -f "$GAME/$f" ] && cp "$GAME/$f" "$OUT/logs/" 2>/dev/null
done
wineserver -k 2>/dev/null
exit $rc
