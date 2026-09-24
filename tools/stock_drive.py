#!/usr/bin/env python3
"""Drives the stock KnC client through the pilot pipe of the dinput8 proxy.

python tools/stock_drive.py walk                 close the welcome popup pick the channel then garage shop mission messenger and back to the lobby
python tools/stock_drive.py room [log]           create a room on the circuit track start it and drive three laps then back to the lobby
python tools/stock_drive.py ghost [log] [ring]   ghost menu circuit track three laps with a scripted drift on every long turn then the ring dump
python tools/stock_drive.py ghostplain [log]     the same run with no drift
python tools/stock_drive.py race [log]           drive the race that is running now on the follow line
python tools/stock_drive.py car                  print the local car telemetry once
python tools/stock_drive.py ring <file>          write the local car ghost ring as a KCGR file after a finish
python tools/stock_drive.py <verb> ...           any raw pilot verb

The client runs at 1024x768 with scale 1 so every point below is a client pixel of that size.
The pilot pipe answers one line per command and the DLL keeps the held keys so a race can hold the gas.
Set KNC_ALLOW_FOCUS=1 to let the script bring the client window to the front before a click.
"""
import ctypes
import math
import os
import sys
import time

PIPE = r'\\.\pipe\knc_pilot'
TRACK_DIR = os.environ.get('KNC_TRACK_DIR', r'Data\Public\World\Race\Race_01')

VK_SHIFT, VK_ESCAPE = 16, 27
VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN = 37, 38, 39, 40

# stage numbers of the stock state machine
STAGE = {'logo': 1, 'title': 2, 'login': 3, 'channel': 4, 'menu': 5, 'garage': 6, 'shop': 7,
         'lobby': 8, 'room': 9, 'game': 11, 'tutorial': 13, 'licence': 14, 'missionmenu': 24,
         'missionrace': 25}

# screen points in client pixels at 1024x768 read off the pilot captures of 2026-09-15
POINT = {
    'welcome_ok': (511, 671),
    'channel_row': (722, 214),
    'top_lobby': (275, 18), 'top_ghost': (395, 18), 'top_quest': (515, 18),
    'top_licence': (640, 18), 'top_mission': (760, 18),
    'bot_shop': (140, 746), 'bot_gacha': (245, 746), 'bot_garage': (430, 746),
    'bot_carcraft': (550, 746), 'bot_roomcraft': (665, 746),
    'bot_options': (941, 746), 'bot_messenger': (972, 746), 'bot_power': (1002, 746),
    'messenger_ok': (499, 649),
    # the create driver popup of a fresh account first driver nameplate the name box and ok
    'create_driver_first': (417, 436), 'create_name_field': (512, 521), 'create_driver_ok': (512, 558),
    # the lobby
    'lobby_speed_single': (443, 128), 'lobby_create': (933, 128),
    # the create room popup the mode arrows cycle Item Single Item Team Speed Single Speed Team
    'create_mode_right': (670, 343), 'create_ok': (452, 488),
    # the room screen
    'room_lobby': (90, 18), 'room_track': (274, 590), 'room_start': (85, 520),
    # the close button centred in a small info box same spot for several boxes
    'info_close': (506, 418),
    # the choice track popup scrolls one theme per arrow tap the four thumbs sit at 270 424 578 732
    'track_theme_right': (840, 229), 'track_theme_4': (732, 229), 'track_ok': (452, 611),
    # the ghost menu five thumbs at 197 350 503 656 810 the circuit theme is the ninth
    'ghost_theme_right': (925, 167), 'ghost_theme_5': (810, 167), 'ghost_start': (877, 651),
    'ghost_result_ok': (492, 556),
}


def pilot(*words, timeout=25.0):
    """One pilot command over the pipe the reply line without the newline"""
    line = ' '.join(str(w) for w in words).encode('ascii', 'replace') + b'\n'
    deadline = time.time() + timeout
    while True:
        try:
            with open(PIPE, 'r+b', buffering=0) as pipe:
                pipe.write(line)
                reply = pipe.read(8192)
            return reply.decode('ascii', 'replace').strip()
        except OSError:
            if time.time() > deadline:
                raise
            time.sleep(0.05)


def field(reply, key, cast=int, default=None):
    for tok in reply.split():
        if tok.startswith(key + '='):
            try:
                return cast(tok[len(key) + 1:])
            except ValueError:
                return default
    return default


def stage():
    return field(pilot('dump'), 'stage', int, -1)


def dump():
    return pilot('dump')


def wait_stage(want, seconds=20.0):
    r = pilot('waitstage', want, int(seconds * 1000), timeout=seconds + 5)
    return r.startswith('ok')


user32 = ctypes.windll.user32


def client_hwnd():
    return int(field(dump(), 'hwnd', lambda s: int(s, 16), 0) or 0)


def focus():
    """The client polls the real cursor so a click only lands with its window in front"""
    if os.environ.get('KNC_ALLOW_FOCUS') != '1':
        return
    h = client_hwnd()
    if not h:
        return
    fg = user32.GetForegroundWindow()
    t1 = user32.GetWindowThreadProcessId(fg, None)
    t2 = user32.GetWindowThreadProcessId(h, None)
    user32.AttachThreadInput(t1, t2, True)
    user32.ShowWindow(h, 9)
    user32.BringWindowToTop(h)
    user32.SetForegroundWindow(h)
    user32.AttachThreadInput(t1, t2, False)


def tap(x, y, settle=0.25):
    focus()
    pilot('move', x, y)
    time.sleep(0.2)
    r = pilot('click', x, y)
    time.sleep(settle)
    return r


def tap_point(name, settle=0.25):
    x, y = POINT[name]
    return tap(x, y, settle)


def key(*vks):
    return pilot('key', *vks)


def shot(path):
    return pilot('shot', path, 'd3d')


def say(msg):
    print(time.strftime('%H:%M:%S'), msg, flush=True)


# the menu walk

def needs_driver(d=None):
    """True once the catalogues load and the account still owns no character"""
    d = d or dump()
    return field(d, 'drivers', int, 0) > 0 and field(d, 'ownchars', int, 0) == 0


def create_first_driver(name):
    """Picks the first driver types the name and confirms the create driver popup"""
    tap_point('create_driver_first', 0.5)
    tap_point('create_name_field', 0.3)
    pilot('text', name)
    time.sleep(0.3)
    tap_point('create_driver_ok', 1.5)
    say('create driver ' + name + ' ' + dump())


def enter_channel(name=None):
    """A channel row needs a double click a fresh driver is made the licence welcome box closed"""
    for attempt in range(4):
        x, y = POINT['channel_row']
        focus()
        pilot('move', x, y)
        time.sleep(0.2)
        pilot('click', x, y)
        time.sleep(0.12)
        pilot('click', x, y)
        created = False
        stuck = 0
        for _ in range(30):
            d = dump()
            st = field(d, 'stage', int, 0)
            if st == 8:
                break
            if not created and needs_driver(d):
                create_first_driver(name or 'Racer')
                created = True
                stuck = 0
            elif st != 4:
                # a driver with no finished tutorial opens on the licence tab with a welcome box
                if field(d, 'dialog', int, 0) == 1:
                    tap_point('info_close', 1.0)
                    stuck = 0
                tap_point('top_lobby', 1.5)
                stuck += 1
                # the licence tab has no click path out until the server clears the tutorial flag
                if stuck >= 3:
                    say('stuck on stage %d with the licence tutorial not cleared ' % st + d)
                    return False
            time.sleep(1.0)
        if wait_stage(8, 12):
            return True
    return False


def go(name, want_stage, seconds=12.0, settle=2.0):
    tap_point(name, settle)
    ok = wait_stage(want_stage, seconds)
    if not ok:
        # a heavy screen can take longer to load under a slow host waiting again sends no new click
        ok = wait_stage(want_stage, seconds)
    say('%-14s stage=%d wanted=%d %s' % (name, stage(), want_stage, 'ok' if ok else 'MISSED'))
    return ok


def walk():
    """Channel screen to lobby then garage shop mission and messenger every stage for the snap tool"""
    say('walk start ' + dump())
    if stage() == 4:
        tap_point('welcome_ok', 1.5)
        time.sleep(1.5)
        tap_point('welcome_ok', 1.5)
        if not enter_channel(os.environ.get('KNC_USER')):
            say('never reached the lobby ' + dump())
            return False
    say('lobby ' + dump())
    time.sleep(2.5)
    go('bot_garage', 6)
    time.sleep(2.5)
    go('top_lobby', 8)
    time.sleep(2.5)
    go('bot_shop', 7)
    time.sleep(2.5)
    go('top_lobby', 8)
    time.sleep(2.5)
    go('top_mission', 24)
    time.sleep(2.5)
    go('top_lobby', 8)
    time.sleep(2.5)
    # the messenger is a panel over the lobby not a stage so the capture is taken here
    tap_point('bot_messenger', 2.5)
    say('messenger ' + dump())
    tap_point('messenger_ok', 1.5)
    go('top_lobby', 8)
    say('walk done ' + dump())
    return True


def create_room():
    """CREATE then the circuit track the host goes back to the lobby 30 s after a bot is seated"""
    if stage() != 8:
        go('top_lobby', 8)
    tap_point('lobby_create', 1.5)
    if field(dump(), 'stage', int, 0) == 8:
        tap_point('create_ok', 1.0)
    if not wait_stage(9, 15):
        say('no room ' + dump())
        return False
    time.sleep(1.5)
    tap_point('room_track', 1.2)
    for _ in range(5):
        tap_point('track_theme_right', 0.5)
    tap_point('track_theme_4', 0.8)
    tap_point('track_ok', 1.2)
    # a slow frame can eat the first press so ok is sent twice like the welcome popup
    tap_point('track_ok', 1.5)
    say('room ready ' + dump())
    return True


def room_race(log=None):
    """Create the room retry start until a bot is seated or 45 s pass then drive the laps"""
    if not create_room():
        return False
    started = False
    tries = 0
    deadline = time.time() + 45.0
    while time.time() < deadline:
        tries += 1
        tap_point('room_start', 1.0)
        if wait_stage(11, 6):
            started = True
            break
        if field(dump(), 'dialog', int, 0) == 1:
            tap_point('info_close', 1.0)
        time.sleep(3.0)
    say('start pressed %d times started=%s' % (tries, started))
    if not started:
        say('race never started ' + dump())
        return False
    race_drive(log=log, scripted=False)
    wait_stage(9, 60)
    time.sleep(2.0)
    tap_point('room_lobby', 1.5)
    return wait_stage(8, 15)


def ghost_race(log=None, ring_path=None, scripted=True):
    """Ghost menu then the circuit theme then START the three laps then the ring dump"""
    if stage() != 23:
        if stage() != 8:
            go('top_lobby', 8)
        go('top_ghost', 23)
        time.sleep(1.5)
    for _ in range(4):
        tap_point('ghost_theme_right', 0.7)
    tap_point('ghost_theme_5', 1.2)
    tap_point('ghost_start', 1.0)
    if not wait_stage(15, 40):
        say('ghost race never started ' + dump())
        return False
    race_drive(log=log, scripted=scripted)
    time.sleep(4.0)
    if ring_path:
        say('ring samples %d into %s' % (ring_dump(ring_path), ring_path))
    tap_point('ghost_result_ok', 2.0)
    return True


# the race

def load_follow_line(path=os.path.join(TRACK_DIR, 'follow_01.ini')):
    rows = []
    with open(path, encoding='ascii', errors='replace') as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) < 4:
                continue
            try:
                rows.append(tuple(float(p) for p in parts[:4]))
            except ValueError:
                continue
    return rows


def wrap_deg(d):
    while d > 180.0:
        d -= 360.0
    while d < -180.0:
        d += 360.0
    return d


def car():
    return pilot('car')


class Keys:
    """Mirror of the held set so the follower only sends the changes"""

    def __init__(self):
        self.down = set()

    def want(self, vks):
        vks = set(vks)
        for vk in sorted(vks - self.down):
            pilot('hold', vk)
        for vk in sorted(self.down - vks):
            pilot('release', vk)
        self.down = vks

    def clear(self):
        pilot('holdmask')
        self.down = set()


class Follower:
    """Follows the follow line of the track with a scripted drift on every long turn"""

    reach = 22.0
    dead = 3.0
    drift_deg = 25.0
    drift_kmh = 50.0
    brake_deg = 70.0
    brake_kmh = 60.0
    # a turn of this many degrees over the next three rows takes a scripted drift
    turn_deg = 25.0
    turn_kmh = 70.0
    # the drift key stays down this long then the gas edge fires the mini turbo
    drift_hold = 0.8
    drift_gap = 3.0

    def __init__(self, line, scripted=True):
        self.line = line
        self.index = 0
        self.keys = Keys()
        self.drift_since = None
        self.release_at = None
        self.gas_off_until = None
        self.stuck_since = None
        self.reverse_until = None
        self.turbos = 0
        self.drifts = 0
        self.moved = False
        self.scripted = scripted
        self.script_side = None
        self.script_until = None
        self.script_last = -10.0

    def turn_ahead(self):
        """Yaw change of the line over the next three rows negative is a left turn"""
        n = len(self.line)
        a = self.line[self.index % n][3]
        b = self.line[(self.index + 3) % n][3]
        return wrap_deg(b - a)

    def start(self, x, y, yaw):
        rad = math.radians(yaw)
        dirx, diry = -math.cos(rad), math.sin(rad)
        best, best_d = None, 1e30
        for i, (px, py, _, _) in enumerate(self.line):
            dx, dy = px - x, py - y
            if dx * dirx + dy * diry <= 0:
                continue
            d = dx * dx + dy * dy
            if d < best_d:
                best, best_d = i, d
        if best is None:
            for i, (px, py, _, _) in enumerate(self.line):
                dx, dy = px - x, py - y
                d = dx * dx + dy * dy
                if d < best_d:
                    best, best_d = i, d
        self.index = best

    def step(self, x, y, yaw, kmh, drift, now):
        n = len(self.line)
        for _ in range(4):
            px, py, _, _ = self.line[self.index % n]
            if math.hypot(px - x, py - y) > self.reach:
                break
            self.index = (self.index + 1) % n
        px, py, _, _ = self.line[self.index % n]
        dx, dy = px - x, py - y
        want = math.degrees(math.atan2(dy, -dx))
        err = wrap_deg(want - yaw)
        mag = abs(err)

        # the stuck timer only counts once the car has driven so the countdown is not a stall
        if kmh > 10.0:
            self.moved = True
        if kmh < 4.0 and self.moved:
            if self.stuck_since is None:
                self.stuck_since = now
        else:
            self.stuck_since = None
        if self.stuck_since is not None and now - self.stuck_since > 3.0 and self.reverse_until is None:
            self.reverse_until = now + 1.2
            self.stuck_since = None
        if self.reverse_until is not None:
            if now < self.reverse_until:
                keys = {VK_DOWN}
                if err > self.dead:
                    keys.add(VK_LEFT)
                elif err < -self.dead:
                    keys.add(VK_RIGHT)
                self.keys.want(keys)
                return err
            self.reverse_until = None

        keys = set()
        steer = None
        if err > self.dead:
            steer = VK_RIGHT
        elif err < -self.dead:
            steer = VK_LEFT
        if steer is not None:
            keys.add(steer)

        # the scripted drift holds the drift key with the steer of the turn side for a fixed time
        if (self.scripted and self.script_until is None and kmh > self.turn_kmh
                and now - self.script_last > self.drift_gap and self.gas_off_until is None):
            turn = self.turn_ahead()
            if abs(turn) > self.turn_deg:
                self.script_side = VK_LEFT if turn < 0 else VK_RIGHT
                self.script_until = now + self.drift_hold
                self.script_last = now
                self.drifts += 1
        if self.script_until is not None:
            if now < self.script_until:
                self.keys.want({VK_UP, VK_SHIFT, self.script_side})
                return err
            # the key goes up then the gas lifts for a moment so the next press is the edge
            self.script_until = None
            self.gas_off_until = now + 0.12
            # the drift turned the nose far inside so the target is picked again from the new heading
            self.start(x, y, yaw)
            px, py, _, _ = self.line[self.index % n]
            err = wrap_deg(math.degrees(math.atan2(py - y, -(px - x))) - yaw)
            mag = abs(err)
            keys = set()
            if err > self.dead:
                keys.add(VK_RIGHT)
            elif err < -self.dead:
                keys.add(VK_LEFT)

        gas = True
        if mag > self.brake_deg and kmh > self.brake_kmh:
            gas = False
            keys.add(VK_DOWN)

        # a wide turn at speed takes the drift key with the steer for a real drift
        if not self.scripted and steer is not None and mag > self.drift_deg and kmh > self.drift_kmh:
            if self.drift_since is None:
                self.drift_since = now
                self.drifts += 1
            keys.add(VK_SHIFT)
        elif self.drift_since is not None:
            # the key goes up before the straight then the gas edge fires the mini turbo
            held = now - self.drift_since
            self.drift_since = None
            if held > 0.4:
                self.release_at = now
                self.gas_off_until = now + 0.12

        if self.gas_off_until is not None:
            if now < self.gas_off_until:
                gas = False
            else:
                self.gas_off_until = None
                self.turbos += 1
        if gas:
            keys.add(VK_UP)
        self.keys.want(keys)
        return err


RACE_STAGES = (11, 15, 25)


def race_drive(laps_wanted=3, max_seconds=420.0, log=None, scripted=True):
    line = load_follow_line()
    fol = Follower(line, scripted)
    # the car record is zero until the track spawned the kart so the first target waits for it
    c = car()
    for _ in range(600):
        c = car()
        if field(c, 'stage', int, -1) in RACE_STAGES and (field(c, 'x', float, 0.0) != 0.0 or field(c, 'y', float, 0.0) != 0.0):
            break
        time.sleep(0.1)
    x, y = field(c, 'x', float, 0.0), field(c, 'y', float, 0.0)
    yaw = field(c, 'yaw', float, 0.0)
    fol.start(x, y, yaw)
    say('race drive from x=%.1f y=%.1f yaw=%.1f target row %d' % (x, y, yaw, fol.index))
    t0 = time.time()
    last_print = 0.0
    last_cp = None
    lap = 0
    laps_done = 0
    while time.time() - t0 < max_seconds:
        now = time.time()
        c = car()
        st = field(c, 'stage', int, -1)
        if st not in RACE_STAGES:
            say('stage left the game ' + c)
            break
        rank = field(c, 'rank', int, -1)
        if rank is not None and rank >= 0:
            say('finished rank %d ' % rank + c)
            break
        x, y = field(c, 'x', float, 0.0), field(c, 'y', float, 0.0)
        yaw = field(c, 'yaw', float, 0.0)
        kmh = field(c, 'kmh', float, 0.0)
        drift = field(c, 'drift', int, 0)
        cp = field(c, 'cp', int, 0)
        if last_cp is not None and cp < last_cp:
            laps_done += 1
            say('lap %d done at %.1f s' % (laps_done, now - t0))
            if laps_done >= laps_wanted:
                say('all laps done ' + c)
                break
        last_cp = cp
        err = fol.step(x, y, yaw, kmh, drift, now)
        if log is not None:
            log.write('%.3f %.2f %.2f %.2f %.1f %d %.2f %d %s\n' % (
                now - t0, x, y, yaw, kmh, drift, err, fol.index, ','.join(str(v) for v in sorted(fol.keys.down))))
        if now - last_print > 2.0:
            last_print = now
            say('t=%5.1f row=%2d err=%6.1f kmh=%5.1f drift=%d cp=%d gauge=%s keys=%s' % (
                now - t0, fol.index, err, kmh, drift, cp, field(c, 'gauge', str, '?'),
                ','.join(str(v) for v in sorted(fol.keys.down))))
        time.sleep(0.05)
    fol.keys.clear()
    say('race drive end drifts=%d turbos=%d ' % (fol.drifts, fol.turbos) + car())


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    verb = argv[1]
    if verb == 'walk':
        return 0 if walk() else 1
    if verb in ('race', 'room', 'ghost', 'ghostplain'):
        log_path = argv[2] if len(argv) > 2 else None
        log = open(log_path, 'w') if log_path else None
        try:
            if verb == 'race':
                race_drive(log=log, scripted=False)
            elif verb == 'room':
                room_race(log=log)
            else:
                ring = argv[3] if len(argv) > 3 else None
                ghost_race(log=log, ring_path=ring, scripted=(verb == 'ghost'))
        finally:
            if log:
                log.close()
        return 0
    if verb == 'car':
        print(car())
        return 0
    if verb == 'ring':
        path = argv[2] if len(argv) > 2 else 'ring.ghost'
        time_ms = int(argv[3]) if len(argv) > 3 else 0
        print('samples', ring_dump(path, time_ms=time_ms), 'into', path)
        return 0
    print(pilot(*argv[1:]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))


# the ring

GAME_BASE = 0x01B19090
CAR_STRIDE = 0xA7260
RING_OFF = 0x3754
RING_COUNT_OFF = 0xA7858
SAMPLE = 28


def peek(addr, n):
    r = pilot('peek', '0x%X' % addr, n)
    if not r.startswith('ok'):
        raise RuntimeError(r)
    return bytes.fromhex(r.split()[3])


def ring_dump(path, track_id=90, char_id=7, name='DaveDebile', time_ms=0, car_kind=3):
    """Writes the local car ghost ring as a KCGR file the same 28 byte samples the client uploads"""
    import struct
    idx = field(car(), 'idx', int, 0)
    base = GAME_BASE + idx * CAR_STRIDE
    count = struct.unpack('<i', peek(base + RING_COUNT_OFF, 4))[0]
    if count <= 0 or count > 24000:
        raise RuntimeError('ring count %d' % count)
    total = count * SAMPLE
    data = b''
    while len(data) < total:
        n = min(256, total - len(data))
        data += peek(base + RING_OFF + len(data), n)
    nm = name.encode('utf-8')
    hdr = b'KCGR' + struct.pack('<IiiiII', 1, track_id, char_id, time_ms, car_kind, count)
    hdr += bytes(44) + bytes(56) + struct.pack('<H', len(nm)) + nm
    with open(path, 'wb') as f:
        f.write(hdr + data)
    return count
