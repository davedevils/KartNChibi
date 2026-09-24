#!/usr/bin/env python3
# runs one scenario against the stock client in the container walk and race reuse tools stock drive py

import ctypes
import json
import os
import socket
import subprocess
import sys
import threading
import time
import types

USAGE = 'usage: scenario.py <boot|lobby|walk|race|refs|factory|paint|mission|antenna|videocheck|slotswap> <outdir>'

T0 = float(os.environ.get('KNC_T0', time.time()))
BRIDGE = ('127.0.0.1', int(os.environ.get('KNC_BRIDGE_PORT', '47017')))
GAME = os.environ.get('KNC_GAME', '/game')
DISPLAY = os.environ.get('DISPLAY', ':99')
BOOT_SECONDS = float(os.environ.get('KNC_BOOT_SECONDS', '300'))
USER = os.environ.get('KNC_USER', 'Racer')

STAGE_NAMES = {
    1: 'logo', 2: 'title', 3: 'login', 4: 'channel', 5: 'menu', 6: 'garage', 7: 'shop', 8: 'lobby', 9: 'room',
    11: 'game', 13: 'tutorial', 14: 'licence', 15: 'ghostrace', 18: 'carfactory', 19: 'roomeditor',
    22: 'scenario', 23: 'ghostmode', 24: 'missionmenu', 25: 'missionrace', 26: 'questmenu',
}
RACE_STAGES = (11, 15, 25)


def stage_name(st):
    return STAGE_NAMES.get(st, 'stage%s' % st)


def since():
    return time.time() - T0


# every thread keeps its own socket to the bridge so a long waitstage never blocks the watcher
_local = threading.local()


def _drop():
    s = getattr(_local, 'sock', None)
    if s is not None:
        try:
            s.close()
        except OSError:
            pass
    _local.sock = None
    _local.buf = b''


def bridge_call(line, wait):
    for attempt in range(2):
        try:
            if getattr(_local, 'sock', None) is None:
                _local.sock = socket.create_connection(BRIDGE, timeout=5)
                _local.buf = b''
            s = _local.sock
            s.settimeout(wait)
            s.sendall(line.encode('ascii', 'replace') + b'\n')
            while b'\n' not in _local.buf:
                chunk = s.recv(65536)
                if not chunk:
                    raise OSError('bridge closed')
                _local.buf += chunk
            reply, _local.buf = _local.buf.split(b'\n', 1)
            return reply.decode('ascii', 'replace').strip()
        except OSError:
            _drop()
            if attempt == 1:
                raise
    return ''


def pilot(*words, timeout=25.0):
    line = ' '.join(str(w) for w in words)
    wait = timeout + 5.0
    if words and words[0] == 'waitstage' and len(words) > 2:
        wait = max(wait, int(words[2]) / 1000.0 + 5.0)
    deadline = time.time() + timeout
    while True:
        try:
            r = bridge_call(line, wait)
            if not r.startswith('err pipe'):
                return r
        except OSError:
            pass
        if time.time() > deadline:
            raise OSError('pilot unreachable for ' + line)
        time.sleep(0.2)


def field(reply, key, cast=int, default=None):
    for tok in reply.split():
        if tok.startswith(key + '='):
            try:
                return cast(tok[len(key) + 1:])
            except ValueError:
                return default
    return default


def cheap_stage():
    # the car verb runs on the pipe thread so polling it costs the game thread nothing
    try:
        return field(pilot('car', timeout=5), 'stage', int, -1)
    except OSError:
        return -1


class Run:
    def __init__(self, out):
        self.out = out
        self.marks = []
        self.stages = []
        self.shots = []
        self.fps = []
        self.notes = []
        self.lock = threading.Lock()
        self.n = 0
        self.finished = None
        self.laps = 0
        self.go_results = []
        self.log = open(os.path.join(out, 'scenario.log'), 'a', buffering=1)

    def say(self, msg):
        line = '%s %7.1f %s' % (time.strftime('%H:%M:%S'), since(), msg)
        print(line, flush=True)
        self.log.write(line + '\n')

    def mark(self, name):
        with self.lock:
            self.marks.append((name, round(since(), 1)))
        self.say('mark %s' % name)

    def flat(self, png):
        r = subprocess.run(['convert', png, '-format', '%[fx:standard_deviation]', 'info:'],
                           capture_output=True, text=True)
        try:
            return float(r.stdout.strip() or '0') < 0.002
        except ValueError:
            return False

    def x11_grab(self, png):
        # the xvfb screen holds nothing but the wine desktop so a root grab is safe
        cmd = ['import', '-display', DISPLAY, '-window', 'root']
        try:
            rect = pilot('bridge', 'rect', timeout=3)
            parts = rect.split()
            if parts[0] == 'ok':
                x, y, w, h = (int(v) for v in parts[1:5])
                if w > 0 and h > 0:
                    cmd += ['-crop', '%dx%d+%d+%d' % (w, h, x, y), '+repage']
        except (OSError, ValueError, IndexError):
            pass
        cmd.append(png)
        r = subprocess.run(cmd, capture_output=True, text=True)
        return r.returncode == 0 and os.path.exists(png)

    def shot(self, label):
        with self.lock:
            self.n += 1
            n = self.n
        st = cheap_stage()
        name = '%02d_%s' % (n, label)
        png = os.path.join(self.out, name + '.png')
        t = time.time()
        method = 'd3d'
        reply = ''
        # a stage that still loads presents nothing for seconds so the d3d grab is tried again
        for attempt in range(4):
            try:
                reply = pilot('shot', 'Z:' + png.replace('/', '\\'), 'd3d', timeout=15)
            except OSError as e:
                reply = 'err %s' % e
            if reply.startswith('ok') or 'did not present' not in reply:
                break
            time.sleep(1.5)
        ok = reply.startswith('ok') and os.path.exists(png)
        if ok and self.flat(png):
            os.replace(png, os.path.join(self.out, name + '_d3d_flat.png'))
            ok = False
            reply += ' flat'
        if not ok:
            method = 'x11'
            ok = self.x11_grab(png)
        ms = int((time.time() - t) * 1000)
        entry = {'file': os.path.basename(png) if ok else None, 'label': label, 'stage': st,
                 'method': method, 'ms': ms, 't': round(since(), 1), 'pilot': reply[:160]}
        with self.lock:
            self.shots.append(entry)
        self.say('shot %s stage=%s %s %d ms %s' % (name, st, method, ms, 'ok' if ok else 'FAILED ' + reply[:120]))
        return ok

    def measure_fps(self, label, seconds=4.0):
        try:
            a = pilot('d3ddiag')
            t = time.time()
            time.sleep(seconds)
            b = pilot('d3ddiag')
        except OSError:
            return
        dt = time.time() - t
        for key in ('present', 'scpresent', 'endscene'):
            d = (field(b, key, int, 0) or 0) - (field(a, key, int, 0) or 0)
            if d > 0:
                self.fps.append({'at': label, 'fps': round(d / dt, 1), 'counter': key})
                self.say('fps %s %.1f by %s' % (label, d / dt, key))
                return
        self.fps.append({'at': label, 'fps': 0.0, 'counter': 'none', 'diag': b})
        self.say('fps %s unknown %s' % (label, b))

    def save(self, result, extra=None):
        data = {'result': result, 'marks': self.marks, 'stages': self.stages, 'shots': self.shots,
                'fps': self.fps, 'notes': self.notes, 'go': self.go_results, 'finished': self.finished,
                'laps': self.laps}
        if extra:
            data.update(extra)
        with open(os.path.join(self.out, 'summary.json'), 'w') as f:
            json.dump(data, f, indent=1)


class Watcher(threading.Thread):
    """Logs every stage change and shoots a stage once it held still for a moment"""

    def __init__(self, run, settle=2.5):
        super().__init__(daemon=True)
        self.run_ = run
        self.settle = settle
        self.auto_shot = True
        self.stop = False
        self.current = None

    def run(self):
        last, since_change, shot_done = None, time.time(), True
        while not self.stop:
            st = cheap_stage()
            now = time.time()
            if st != last and st != -1:
                self.run_.stages.append((st, stage_name(st), round(since(), 1)))
                self.run_.say('stage %d %s' % (st, stage_name(st)))
                last, since_change, shot_done = st, now, False
                self.current = st
            elif not shot_done and now - since_change > self.settle:
                shot_done = True
                if self.auto_shot:
                    self.run_.shot('%s' % stage_name(st))
            time.sleep(0.25)


def wait_pilot(run, seconds):
    deadline = time.time() + seconds
    pipe_seen = False
    while time.time() < deadline:
        try:
            r = bridge_call('dump', 30)
        except OSError:
            time.sleep(0.5)
            continue
        if r.startswith('ok'):
            run.mark('pilot_dump_ok')
            run.say('dump ' + r)
            run.notes.append('first dump ' + r)
            return True
        if not r.startswith('err pipe') and not pipe_seen:
            pipe_seen = True
            run.mark('pilot_pipe_up')
            run.say('pipe answered ' + r)
        time.sleep(0.5)
    return False


def wait_stage_poll(want, seconds, run=None):
    """Polls the stage and gives up early on a refusal box held over the title"""
    deadline = time.time() + seconds
    boxed_since = None
    while time.time() < deadline:
        if cheap_stage() == want:
            return True
        if run is not None:
            try:
                d = pilot('dump', timeout=10)
            except OSError:
                d = ''
            if field(d, 'stage', int, -1) == 2 and field(d, 'dialog', int, 0) == 1:
                boxed_since = boxed_since or time.time()
                if time.time() - boxed_since > 20.0:
                    run.shot('refused_box')
                    run.notes.append('login refused, a box stayed over the title, see the refused_box shot and packets log')
                    run.say('refusal box over the title ' + d)
                    return False
            else:
                boxed_since = None
        time.sleep(0.5)
    return False


def load_driver(run):
    sys.path.insert(0, '/opt/knc/tools')
    if not hasattr(ctypes, 'windll'):
        ctypes.windll = types.SimpleNamespace(user32=None)
    import stock_drive as sd
    sd.pilot = pilot
    orig_say = sd.say
    orig_go = sd.go
    orig_drive = sd.race_drive
    orig_follow = sd.load_follow_line

    def say(msg):
        run.say('drive ' + msg)
        if msg.startswith('create driver'):
            run.shot('driver_created')
        elif msg.startswith('stuck on stage'):
            run.notes.append(msg)
            run.shot('licence_stuck')
        elif msg.startswith('messenger'):
            run.shot('messenger')
        elif msg.startswith('room ready'):
            run.shot('room_track_set')
        elif msg.startswith('finished rank'):
            run.finished = msg
            run.shot('finish')
        elif msg.startswith('all laps done'):
            run.finished = run.finished or msg
        elif msg.startswith('lap '):
            run.laps += 1
            run.shot('lap')

    def go(name, want_stage, seconds=12.0, settle=2.0):
        ok = orig_go(name, want_stage, seconds, settle)
        run.go_results.append((name, want_stage, bool(ok)))
        run.shot('%s_%s' % (name, stage_name(want_stage)))
        return ok

    def follow(path=os.path.join(GAME, 'Data', 'Public', 'World', 'Race', 'Race_01', 'follow_01.ini')):
        return orig_follow(path)

    def race_drive(*a, **k):
        run.mark('race_drive_start')
        stopper = {'stop': False}

        def periodic():
            # the fps probe waits for the track load since a game thread verb must not meet a long load
            n = 0
            while not stopper['stop'] and n < 12:
                time.sleep(20.0)
                if stopper['stop']:
                    break
                if cheap_stage() in RACE_STAGES:
                    run.shot('race_%02d' % n)
                    if n == 0:
                        run.measure_fps('race', 3.0)
                    n += 1

        t = threading.Thread(target=periodic, daemon=True)
        t.start()
        try:
            return orig_drive(*a, **k)
        finally:
            stopper['stop'] = True
            run.mark('race_drive_end')
            # the result board shows inside the race stage before the room comes back
            for i in range(8):
                st = cheap_stage()
                run.shot('result_%d' % i)
                if st not in RACE_STAGES:
                    break
                time.sleep(4.0)

    sd.say = say
    sd.go = go
    sd.load_follow_line = follow
    sd.race_drive = race_drive
    sd.TRACK_DIR = os.path.join(GAME, 'Data', 'Public', 'World', 'Race', 'Race_01')
    return sd


INFO_CLOSE = (506, 418)


def room_race(run, sd, log):
    """The room race of stock drive with START pressed again until the server seats a bot"""
    if not sd.create_room():
        return False
    run.mark('room_ready')
    started = False
    deadline = time.time() + 75.0
    tries = 0
    while time.time() < deadline:
        tries += 1
        sd.tap_point('room_start', 1.0)
        if sd.wait_stage(11, 6):
            started = True
            break
        d = pilot('dump')
        if field(d, 'dialog', int, 0) == 1:
            # the stock client refuses a start alone so the box is closed and START pressed again
            if tries == 1:
                run.shot('start_refused_alone')
            sd.tap(INFO_CLOSE[0], INFO_CLOSE[1], 1.0)
        time.sleep(3.0)
    run.say('start pressed %d times started=%s' % (tries, started))
    if not started:
        run.notes.append('race never started after %d START presses' % tries)
        return False
    run.mark('race_stage11')
    sd.race_drive(log=log, scripted=False)
    back = sd.wait_stage(9, 90)
    run.mark('room_again' if back else 'no_room_after_race')
    time.sleep(2.0)
    run.shot('room_after_race')
    # a slow frame was seen to eat this click so the lobby tab is retried with any box closed first
    ok = False
    deadline = time.time() + 60.0
    while time.time() < deadline:
        d = pilot('dump')
        if field(d, 'dialog', int, 0) == 1:
            sd.tap(INFO_CLOSE[0], INFO_CLOSE[1], 1.0)
        sd.tap_point('room_lobby', 1.5)
        if sd.wait_stage(8, 6):
            ok = True
            break
    if ok:
        run.mark('lobby_after_race')
    else:
        run.notes.append('room lobby click never reached stage 8 after the race')
    return ok


# capture plan spots of the UI parity doc in stock 1024x768 client pixels
CLOSE_PENDANT = (745, 690)      # Buttons Common Close of the pendant detail box
CLOSE_MESSENGER = (499, 649)    # Common OK1 of the messenger list box read off a real capture
QUIT_CONFIRM_OK = (463, 418)    # OK of the Sure you want to quit box read off a real capture
ITEM_POPUP_CLOSE = (575, 565)   # Shop Popup Close half read off the Gold Coin box capture


def has_dialog():
    return field(pilot('dump', timeout=8), 'dialog', int, 0) == 1


def close_any_dialog(sd, tries=3):
    """Escape closes every stock popup seen so far used after a click that may open one"""
    for _ in range(tries):
        if not has_dialog():
            return True
        sd.key('esc')
        time.sleep(0.8)
    return not has_dialog()


def quit_run_confirm(sd):
    """Escape on a race like stage opens Sure you want to quit OK leaves it retried once"""
    sd.key('esc')
    time.sleep(1.5)
    for _ in range(2):
        sd.tap(*QUIT_CONFIRM_OK, settle=1.5)
        if not has_dialog():
            break
        time.sleep(0.8)
    close_any_dialog(sd)


def step(run, name, fn):
    """Runs one capture step logs and moves on so one miss does not stop the rest of the plan"""
    try:
        fn()
        run.say('refs step %s done' % name)
        return True
    except Exception as e:
        run.notes.append('refs step %s failed: %r, dump %s' % (name, e, pilot('dump', timeout=5)))
        run.say('refs step %s FAILED %r' % (name, e))
        return False


def to_lobby(sd, run):
    close_any_dialog(sd)
    if cheap_stage() != 8:
        sd.tap_point('top_lobby', 1.5)
        if not sd.wait_stage(8, 15):
            close_any_dialog(sd)
            sd.tap_point('top_lobby', 1.5)
            sd.wait_stage(8, 15)
    time.sleep(0.5)


def capture_reference_plan(run, sd):
    """Walks every screen of the capture plan table in one login and shoots it with the pilot"""
    ok_count = [0]
    total = [0]

    def s(name, fn):
        total[0] += 1
        if step(run, name, fn):
            ok_count[0] += 1

    # ---- shop bundle character car item room craft and car craft tabs one visit ----
    def shop_open():
        to_lobby(sd, run)
        sd.tap_point('bot_shop', 1.5)
        sd.wait_stage(7, 15)
    s('shop_open', shop_open)

    def shop_antenna():
        sd.tap(586, 112, 1.0)   # Car tab
        sd.tap(597, 147, 1.0)   # Antenna sub tab
        run.shot('shop_antenna')
    s('shop_antenna', shop_antenna)

    def shop_item():
        sd.tap(680, 112, 1.0)   # Item tab
        run.shot('shop_item')
    s('shop_item', shop_item)

    def shopgift():
        sd.tap(656, 387, 1.0)   # Gift half of the Gacha Coin tile
        sd.tap(634, 312, 0.8)   # the drop arrow lists the friends
        run.shot('shopgift')
        # the dump dialog flag does not cover this box its own Close half does not send
        sd.tap(*ITEM_POPUP_CLOSE, settle=1.0)
    s('shopgift', shopgift)

    def gacha_coin_buy():
        sd.tap(725, 387, 1.0)   # Buy on the Gacha Coin tile
        sd.tap(482, 565, 1.5)   # the Buy half buys the selected first row
        close_any_dialog(sd)    # a real refusal box not enough gold is flagged and needs a close
    s('gacha_coin_buy', gacha_coin_buy)

    def shop_goldcoin():
        # Gold Coin is base key 2001 the tile right after the Gacha Coin one on the same row
        sd.tap(894, 387, 1.0)
        run.shot('shop_goldcoin')
        sd.tap(*ITEM_POPUP_CLOSE, settle=1.0)
    s('shop_goldcoin', shop_goldcoin)

    def shop_roomcraft():
        sd.tap(780, 112, 1.0)   # Room Craft tab
        run.shot('shop_roomcraft')
        sd.tap(733, 147, 1.0)   # Object sub tab
        sd.tap(540, 385, 1.0)   # a tile
        sd.tap(436, 551, 1.5)   # Buy needed later for the room craft field
        close_any_dialog(sd)    # a real refusal box not enough gold is flagged and needs a close
    s('shop_roomcraft', shop_roomcraft)

    def shop_carcraft():
        sd.tap(880, 112, 1.0)   # Car Craft tab
        run.shot('shop_carcraft')
    s('shop_carcraft', shop_carcraft)

    to_lobby(sd, run)
    close_any_dialog(sd)

    # ---- menu gear popups ----
    def escmenu():
        sd.tap(942, 748, 1.0)
        run.shot('escmenu')
    s('escmenu', escmenu)

    def help_sheet():
        sd.tap(460, 302, 1.0)
        run.shot('help')
        sd.key('esc')
        time.sleep(0.8)
    s('help', help_sheet)

    def gameoption():
        sd.tap(942, 748, 1.0)
        sd.tap(460, 338, 1.0)
        run.shot('gameoption')
    s('gameoption', gameoption)

    def gameoption_sound():
        sd.tap(527, 221, 1.0)   # Sound tab
        sd.tap(392, 389, 0.6)   # effect bar minus
        run.shot('gameoption_sound')
        sd.tap(549, 580, 1.0)   # OK closes the panel
    s('gameoption_sound', gameoption_sound)

    def controloption():
        sd.tap(942, 748, 1.0)
        sd.tap(460, 374, 1.0)
        run.shot('controloption')
        sd.key('esc')
        time.sleep(0.5)
        sd.tap(532, 550, 1.0)   # OK closes the panel
    s('controloption', controloption)

    def escconfirm():
        sd.tap(942, 748, 1.0)
        sd.tap(460, 410, 1.0)
        run.shot('escconfirm')
        sd.tap(513, 405, 1.0)   # Cancel stay in the client
    s('escconfirm', escconfirm)

    # ---- garage equip ----
    def garage_equip():
        to_lobby(sd, run)
        sd.tap_point('bot_garage', 1.5)
        sd.wait_stage(6, 15)
        sd.tap(529, 110, 1.0)   # Character tab
        sd.tap(648, 229, 1.0)   # a cell not worn
        sd.tap(736, 692, 1.5)   # Equip
        run.shot('garage_equip')
    s('garage_equip', garage_equip)

    # ---- pendant box off the char panel ----
    def pendant():
        to_lobby(sd, run)
        sd.tap(333, 176, 1.0)
        run.shot('pendant')
        sd.tap(*CLOSE_PENDANT, settle=0.8)
        close_any_dialog(sd)
    s('pendant', pendant)

    # ---- gacha idle spin and prize needs the coin bought above ----
    def gacha():
        to_lobby(sd, run)
        sd.tap_point('bot_gacha', 1.5)
        run.shot('gacha')
        # the shop bundle bought a Gold Coin so the Gold tab holds the roll not the default Astro one
        sd.tap(905, 470, 1.0)
    s('gacha', gacha)

    def gacha_spin_prize():
        sd.tap(506, 606, 1.0)   # roll
        time.sleep(4.0)
        run.shot('gacha_spin')
        time.sleep(6.0)
        run.shot('gacha_prize')
        # a won item shows its own You have received an item OK box over the machine first
        sd.tap(658, 538, 1.5)   # OK on that box a miss here lands on empty machine space
        sd.tap(731, 607, 1.5)   # Close of the Crazy Lotto box
        close_any_dialog(sd)
    s('gacha_spin_prize', gacha_spin_prize)

    # ---- room craft field the object was bought in the shop bundle above ----
    def roomcraft():
        to_lobby(sd, run)
        sd.tap_point('bot_roomcraft', 1.5)
        reached = sd.wait_stage(19, 15)
        if not reached:
            sd.tap_point('bot_roomcraft', 1.5)   # a second press in case the first was a focus click
            reached = sd.wait_stage(19, 15)
        if not reached:
            sd.tap(606, 731, 1.5)   # the capture plan spot tried as a fallback
            reached = sd.wait_stage(19, 20)
        if not reached:
            # a bare goto 19 crashed the client once so a miss is reported not forced
            run.notes.append('room craft never reached stage 19 dump ' + pilot('dump', timeout=5))
            run.shot('roomcraft')   # whatever is on screen so the miss is visible
            return
        sd.tap(590, 106, 1.0)    # Object kind tab
        sd.tap(241, 132, 1.0)    # the first strip tile
        sd.tap(500, 560, 1.5)    # places it on the field shows the red marker
        run.shot('roomcraft')
        # escape here opens the Menu popup so the next step leaves through the top bar Lobby tab
    s('roomcraft', roomcraft)

    # ---- car factory the Circler chassis and its seven parts come from the db fixture ----
    def carcraft():
        to_lobby(sd, run)
        sd.tap_point('bot_carcraft', 1.5)
        reached = sd.wait_stage(18, 15)
        if not reached:
            run.notes.append('car craft never reached stage 18 dump ' + pilot('dump', timeout=5))
        time.sleep(1.5)
        run.shot('carcraft')
        # same as room craft no escape here the top bar Lobby tab leaves cleanly next step
    s('carcraft', carcraft)

    # ---- licence picked state and a run ----
    def licence():
        to_lobby(sd, run)
        sd.tap_point('top_licence', 1.5)
        sd.wait_stage(14, 15)
        close_any_dialog(sd)    # a fresh account shows a one time welcome box on this tab
        sd.tap(372, 317, 1.5)    # a tile of the open tier
        run.shot('licence_pick')
        sd.tap(108, 267, 1.5)    # Start the round button read off the picked state capture
        time.sleep(1.5)
        sd.key('enter')          # closes the mummy board
        reached = sd.wait_stage(13, 20)
        if not reached:
            run.notes.append('licence test never reached stage 13 dump ' + pilot('dump', timeout=5))
        time.sleep(3.0)
        run.shot('licence_race')
        quit_run_confirm(sd)
        sd.wait_stage(14, 15)
    s('licence', licence)

    # ---- messenger row menu ----
    def messenger_rowmenu():
        to_lobby(sd, run)
        sd.tap_point('bot_messenger', 1.5)
        time.sleep(1.0)
        sd.tap(380, 232, 1.0)    # a friend row
        run.shot('messenger_rowmenu')
        sd.tap(*CLOSE_MESSENGER, settle=1.0)
        sd.tap(*CLOSE_MESSENGER, settle=1.0)   # a second press in case the first missed
        close_any_dialog(sd)
    s('messenger_rowmenu', messenger_rowmenu)

    # ---- mission run board ----
    def mission_board():
        to_lobby(sd, run)
        sd.tap_point('top_mission', 1.5)
        sd.wait_stage(24, 15)
        sd.tap(300, 230, 1.0)    # a row
        sd.tap(457, 587, 1.0)    # Start
        time.sleep(1.0)
        if has_dialog():
            sd.tap_point('info_close', 1.0)  # the mission fee confirm box OK is this spot
        reached = sd.wait_stage(25, 20)
        if not reached:
            run.notes.append('mission run never reached stage 25 dump ' + pilot('dump', timeout=5))
        time.sleep(1.5)
        sd.key('enter')          # closes the mummy board
        time.sleep(0.5)
        run.shot('mission_board')
        quit_run_confirm(sd)
    s('mission_board', mission_board)

    to_lobby(sd, run)
    run.say('refs plan %d of %d steps ok' % (ok_count[0], total[0]))
    return ok_count[0] >= total[0] // 2


# stage 18 spots read off the init of sub 430EF0 and the row hit tests of sub 4325D0
FACTORY_NAME_PLATE = (486, 245)
FACTORY_TAB = {'chassis': (91, 267), 'cover': (169, 267), 'tire': (247, 267), 'booster': (325, 267),
               'bumper': (91, 346), 'ffender': (169, 346), 'rfender': (247, 346), 'wing': (325, 346)}
FACTORY_ROWS = [(200, 465), (200, 529), (200, 593)]
FACTORY_ROW0_INFO = (351, 482)
FACTORY_EQUIP = (160, 686)
FACTORY_REMOVE = (272, 686)
FACTORY_SAVE = (902, 682)
# top row slot hit boxes of sub 4325D0 x 87 plus 212 per slot y 107 to 202
FACTORY_SLOTS = [(190, 150), (402, 150), (614, 150), (826, 150)]
MENU_CLOSE = (485, 456)
# garage spots read off sub 413340 the Car category the Paint tab the first cell and Remove
GARAGE_CAR_TAB = (628, 110)
GARAGE_PAINT_TAB = (774, 146)
GARAGE_CELL0 = (486, 229)
GARAGE_REMOVE = (736, 692)


def crash_box():
    """Title of a runtime error box on the X screen or an empty string"""
    try:
        r = subprocess.run(['xwininfo', '-root', '-tree', '-display', DISPLAY],
                           capture_output=True, text=True, timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        return ''
    for line in r.stdout.splitlines():
        if 'Runtime Library' in line or 'Buffer overrun' in line or 'Program Error' in line:
            return line.strip()
    return ''


def winedbg_script(lines, timeout=120):
    try:
        r = subprocess.run(['winedbg'], input='\n'.join(lines) + '\n', capture_output=True,
                           text=True, timeout=timeout)
        return r.stdout + r.stderr
    except (OSError, subprocess.TimeoutExpired) as e:
        return 'winedbg failed %r' % e


def dump_client_stack(run):
    """Attaches winedbg to KnC exe and writes every thread backtrace next to the shots"""
    procs = winedbg_script(['info proc', 'quit'], 60)
    pid = None
    for line in procs.splitlines():
        if 'KnC.exe' in line:
            for tok in line.split():
                try:
                    pid = int(tok, 16)
                    break
                except ValueError:
                    continue
            if pid:
                break
    text = procs
    if pid:
        text += '\n' + winedbg_script(['attach 0x%x' % pid, 'bt all', 'detach', 'quit'], 180)
    path = os.path.join(run.out, 'client_stack.txt')
    with open(path, 'w') as f:
        f.write(text)
    run.say('client stack pid %s written %d bytes' % (pid, len(text)))
    return text


def factory_box(run, label):
    """Waits a moment for a runtime error box and dumps the client stack when one shows"""
    for _ in range(6):
        box = crash_box()
        if box:
            run.mark('client_runtime_box')
            run.notes.append('runtime box after %s %s' % (label, box))
            run.x11_grab(os.path.join(run.out, 'zz_runtime_box.png'))
            dump_client_stack(run)
            return True
        time.sleep(1.0)
    return False


def factory_open(run, sd, label):
    to_lobby(sd, run)
    sd.tap_point('bot_carcraft', 1.5)
    if not sd.wait_stage(18, 20):
        run.notes.append('car craft never reached stage 18 dump ' + pilot('dump', timeout=5))
        run.shot('%s_missed' % label)
        return False
    time.sleep(2.0)
    run.shot(label)
    return True


def factory_leave(run, sd):
    """Escape opens the Menu popup here so a box is closed by its button then the Lobby tab"""
    for _ in range(3):
        if has_dialog():
            sd.tap(*INFO_CLOSE, settle=1.0)
        sd.tap(*MENU_CLOSE, settle=0.8)
        sd.tap_point('top_lobby', 1.5)
        if sd.wait_stage(8, 15):
            return True
    return False


def factory_equip(run, sd, tab, row, label):
    sd.tap(*FACTORY_TAB[tab], settle=1.0)
    sd.tap(*FACTORY_ROWS[row], settle=1.0)
    sd.tap(*FACTORY_EQUIP, settle=2.0)
    run.shot(label)


def factory_probe(run, sd):
    """Car factory top row of owned chassis, the second slot, install remove save, the name plate, the garage"""
    if not factory_open(run, sd, 'factory_open'):
        return False
    sd.tap(*FACTORY_SLOTS[1], settle=2.0)
    run.shot('factory_slot2')
    sd.tap(*FACTORY_TAB['tire'], settle=1.0)
    run.shot('factory_slot2_tire_list')
    sd.tap(*FACTORY_SLOTS[0], settle=2.0)
    run.shot('factory_slot1')
    for name in ('cover', 'tire', 'booster'):
        sd.tap(*FACTORY_TAB[name], settle=1.0)
        run.shot('factory_%s_list' % name)
    factory_equip(run, sd, 'chassis', 0, 'factory_chassis_equipped')
    factory_equip(run, sd, 'tire', 0, 'factory_tire_equipped')
    for row, label in ((1, 'factory_cover_3days'), (2, 'factory_cover_1day'), (0, 'factory_cover_permanent')):
        factory_equip(run, sd, 'cover', row, label)
    sd.tap(*FACTORY_REMOVE, settle=2.0)
    run.shot('factory_cover_removed')
    factory_equip(run, sd, 'cover', 1, 'factory_cover_3days_again')
    sd.tap(*FACTORY_ROW0_INFO, settle=1.5)
    run.shot('factory_part_popup')
    sd.tap(*ITEM_POPUP_CLOSE, settle=1.0)
    if factory_box(run, 'the part popup'):
        return False
    sd.tap(*FACTORY_SAVE, settle=3.0)
    run.shot('factory_saved')
    if has_dialog():
        sd.tap(*INFO_CLOSE, settle=1.0)
    if factory_box(run, 'the save'):
        return False
    run.mark('name_plate_click')
    sd.tap(*FACTORY_NAME_PLATE, settle=2.0)
    if factory_box(run, 'the name plate click'):
        return False
    run.shot('factory_rename_dialog')
    pilot('text', 'DockKart9')
    time.sleep(0.5)
    sd.key('enter')
    time.sleep(2.0)
    run.shot('factory_renamed')
    if factory_box(run, 'the rename'):
        return False
    if not factory_leave(run, sd):
        run.notes.append('no lobby after the first factory visit')
        return False
    run.mark('lobby_after_factory')
    if not factory_open(run, sd, 'factory_reopen'):
        return False
    sd.tap(*FACTORY_NAME_PLATE, settle=2.0)
    if factory_box(run, 'the second name plate click'):
        return False
    run.shot('factory_rename_dialog_again')
    sd.key('esc')
    time.sleep(1.0)
    back = factory_leave(run, sd)
    run.mark('lobby_after_reopen' if back else 'no_lobby_after_reopen')
    if not back or crash_box():
        return False
    return garage_factory_karts(run, sd)


def garage_factory_karts(run, sd):
    """Garage Car category the crafted karts first their detail box lists the parts and the wrench bar"""
    sd.tap_point('bot_garage', 1.5)
    if not sd.wait_stage(6, 20):
        run.notes.append('garage never reached stage 6 dump ' + pilot('dump', timeout=5))
        return False
    time.sleep(1.5)
    sd.tap(*GARAGE_CAR_TAB, settle=1.5)
    run.shot('garage_kart_tab')
    for i in range(2):
        sd.tap(GARAGE_CELL0[0] + 81 * i, GARAGE_CELL0[1], settle=1.5)
        run.shot('garage_factory_kart%d' % i)
    run.notes.append('karts at the garage ' + pilot('karts', timeout=8))
    box = crash_box()
    sd.tap_point('top_lobby', 1.5)
    back = sd.wait_stage(8, 20)
    run.mark('lobby_after_garage' if back else 'no_lobby_after_garage')
    return back and not box


def paint_probe(run, sd):
    """Garage Car category Paint tab Remove on the worn paint the answer the kart gets"""
    to_lobby(sd, run)
    sd.tap_point('bot_garage', 1.5)
    if not sd.wait_stage(6, 20):
        run.notes.append('garage never reached stage 6 dump ' + pilot('dump', timeout=5))
        return False
    time.sleep(1.5)
    sd.tap(*GARAGE_CAR_TAB, settle=1.0)
    sd.tap(*GARAGE_PAINT_TAB, settle=1.5)
    run.shot('garage_paint_tab')
    sd.tap(*GARAGE_CELL0, settle=1.5)
    run.shot('garage_paint_selected')
    run.mark('paint_remove_click')
    sd.tap(*GARAGE_REMOVE, settle=3.0)
    run.shot('garage_paint_removed')
    d = pilot('dump', timeout=8)
    run.notes.append('dump after remove ' + d)
    boxed = field(d, 'dialog', int, 0) == 1
    if boxed:
        run.notes.append('a box is up after the paint remove')
        run.shot('garage_paint_box')
        close_any_dialog(sd)
    box = crash_box()
    if box:
        run.notes.append('runtime box after the paint remove ' + box)
        dump_client_stack(run)
        return False
    sd.tap_point('top_lobby', 1.5)
    back = sd.wait_stage(8, 20)
    time.sleep(2.0)
    run.shot('lobby_after_paint')
    return back and not boxed


MISSION_ROW0 = (95, 245)        # the first medal of the mission menu slots read off a stock capture
MISSION_START = (490, 617)      # the middle of the Start sprite drawn from 457 587 on a playable row
MISSION_DOWN = (95, 693)        # the down arrow pages five rows


def mission_probe(run, sd):
    """Mission menu frames a moment apart then mission 0 its board the run and the menu again"""
    to_lobby(sd, run)
    run.notes.append('karts at the lobby ' + pilot('karts', timeout=8))
    sd.tap_point('top_mission', 1.5)
    if not sd.wait_stage(24, 20):
        run.notes.append('mission menu never reached stage 24 dump ' + pilot('dump', timeout=5))
        run.shot('mission_missed')
        return False
    run.mark('mission_menu')
    time.sleep(2.0)
    for i in range(4):
        run.shot('missionmenu_%d' % i)
        time.sleep(0.6)
    run.notes.append('karts at the mission menu ' + pilot('karts', timeout=8))
    sd.tap(*MISSION_DOWN, settle=1.0)
    run.shot('missionmenu_page2')
    sd.tap(95, 175, 1.0)            # the up arrow back to the first page
    sd.tap(*MISSION_ROW0, settle=1.0)
    run.shot('missionmenu_row0')
    # the Start button record state x y w h id at stage 0xD09EB0 plus 0x243C plus two records
    run.notes.append('start button before ' + pilot('peek', '0xD0C6BC', 24, timeout=8))
    run.notes.append('picked row ' + pilot('peek', '0xD121FC', 4, timeout=8))
    pilot('move', *MISSION_START)
    time.sleep(0.6)
    sd.tap(*MISSION_START, settle=1.5)
    run.notes.append('start button after ' + pilot('peek', '0xD0C6BC', 24, timeout=8))
    run.notes.append('fee popup flag ' + pilot('peek', '0xEAB880', 1, timeout=8))
    run.shot('mission_fee_question')
    # the confirm of MISSION ENTER takes Enter as its OK and sends 0x0090
    sd.key('enter')
    time.sleep(1.0)
    if has_dialog():
        sd.tap(*QUIT_CONFIRM_OK, settle=1.0)
    reached = sd.wait_stage(25, 25)
    if not reached:
        run.notes.append('mission run never reached stage 25 dump ' + pilot('dump', timeout=5))
        run.shot('mission_run_missed')
        close_any_dialog(sd)
        return False
    run.mark('mission_run')
    time.sleep(2.5)
    for i in range(3):
        run.shot('mission_board_%d' % i)
        time.sleep(0.6)
    sd.key('enter')                 # closes the mummy board and arms the countdown
    time.sleep(4.0)
    for i in range(3):
        run.shot('mission_run_%d' % i)
        time.sleep(0.6)
    quit_run_confirm(sd)
    back = sd.wait_stage(24, 25)
    run.mark('mission_menu_again' if back else 'no_mission_menu_after_run')
    time.sleep(2.0)
    for i in range(2):
        run.shot('missionmenu_after_%d' % i)
        time.sleep(0.6)
    run.notes.append('karts after the run ' + pilot('karts', timeout=8))
    to_lobby(sd, run)
    run.shot('lobby_after_mission')
    return back


GARAGE_ANTENNA_TAB = (592, 146)  # Shop Common Car Tab Antenna drawn at 552 134 by sub 413340
GARAGE_INSTALL = (736, 692)


def burst_shots(run, label, count=3, gap=0.5):
    """A few frames of one screen a moment apart so a moving antenna shows in their difference"""
    for i in range(count):
        run.shot('%s_%d' % (label, i))
        time.sleep(gap)


def antenna_probe(run, sd):
    """Garage antenna install then the lobby the licence pick and the mission menu the preview screens"""
    to_lobby(sd, run)
    burst_shots(run, 'lobby_before')
    run.notes.append('karts before ' + pilot('karts', timeout=8))
    sd.tap_point('bot_garage', 1.5)
    if not sd.wait_stage(6, 20):
        run.notes.append('garage never reached stage 6 dump ' + pilot('dump', timeout=5))
        return False
    time.sleep(1.5)
    sd.tap(*GARAGE_CAR_TAB, settle=1.0)
    sd.tap(*GARAGE_ANTENNA_TAB, settle=1.5)
    run.shot('garage_antenna_tab')
    sd.tap(*GARAGE_CELL0, settle=1.5)
    run.shot('garage_antenna_picked')
    run.mark('antenna_install_click')
    sd.tap(*GARAGE_INSTALL, settle=3.0)
    close_any_dialog(sd)
    burst_shots(run, 'garage_after_install')
    to_lobby(sd, run)
    time.sleep(1.5)
    burst_shots(run, 'lobby_after_install')
    sd.tap_point('top_licence', 1.5)
    if sd.wait_stage(14, 15):
        close_any_dialog(sd)
        sd.tap(372, 317, 1.5)
        burst_shots(run, 'licence_pick')
    to_lobby(sd, run)
    sd.tap_point('top_mission', 1.5)
    if sd.wait_stage(24, 15):
        time.sleep(1.5)
        burst_shots(run, 'missionmenu')
    to_lobby(sd, run)
    time.sleep(1.5)
    burst_shots(run, 'lobby_end')
    run.notes.append('karts after ' + pilot('karts', timeout=8))
    return True


SHOP_ITEM_TAB = (680, 112)
SHOP_ITEM_TILES = ((558, 387), (727, 387), (895, 387))   # the Buy halves of the first Item row open the buy box


def videocheck_probe(run, sd):
    """The screens of the official server video the char panel the Item tab its three tiles and the missions"""
    to_lobby(sd, run)
    time.sleep(1.5)
    run.shot('lobby_charpanel')
    sd.tap_point('bot_shop', 1.5)
    if not sd.wait_stage(7, 20):
        run.notes.append('shop never reached stage 7 dump ' + pilot('dump', timeout=5))
        return False
    time.sleep(1.5)
    sd.tap(*SHOP_ITEM_TAB, settle=1.5)
    run.shot('shop_item_tab')
    for i, spot in enumerate(SHOP_ITEM_TILES):
        sd.tap(*spot, settle=1.5)
        run.shot('shop_item_tile_%d' % i)
        sd.tap(*ITEM_POPUP_CLOSE, settle=1.0)
        close_any_dialog(sd)
    to_lobby(sd, run)
    sd.tap_point('top_mission', 1.5)
    if not sd.wait_stage(24, 20):
        run.notes.append('mission menu never reached stage 24 dump ' + pilot('dump', timeout=5))
        return False
    time.sleep(2.0)
    burst_shots(run, 'missionmenu')
    for i, y in enumerate((245, 340, 433)):
        sd.tap(95, y, 1.0)
        run.shot('missionmenu_row%d' % i)
    to_lobby(sd, run)
    return True


def slotswap_probe(run, sd, log):
    """A room race where Alt is pressed every few seconds so two held items swap on the Slot Exchange"""
    stopper = {'stop': False}
    presses = [0]

    def alt_loop():
        while not stopper['stop']:
            time.sleep(4.0)
            if cheap_stage() not in RACE_STAGES:
                continue
            try:
                pilot('hold', 18, timeout=5)
                time.sleep(0.25)
                pilot('release', 18, timeout=5)
                presses[0] += 1
            except OSError:
                pass

    t = threading.Thread(target=alt_loop, daemon=True)
    t.start()
    try:
        ok = room_race(run, sd, log)
    finally:
        stopper['stop'] = True
    run.notes.append('alt presses %d' % presses[0])
    return ok


def do_boot(run, watcher, until_channel=True):
    if not wait_pilot(run, BOOT_SECONDS):
        run.say('pilot never answered dump in %d s' % BOOT_SECONDS)
        return False
    watcher.start()
    got = run.shot('boot_first')
    try:
        run.say('stage ' + pilot('stage'))
        run.say('d3ddiag ' + pilot('d3ddiag'))
    except OSError:
        pass
    run.measure_fps('boot', 3.0)
    if not until_channel:
        return got
    if wait_stage_poll(4, BOOT_SECONDS, run):
        run.mark('channel_stage4')
        time.sleep(3.0)
        run.measure_fps('channel', 4.0)
    else:
        run.say('no channel screen, dump ' + pilot('dump'))
        run.notes.append('channel stage 4 not reached')
    return got


def do_lobby(run, watcher, sd):
    if not do_boot(run, watcher):
        return False
    if cheap_stage() != 4:
        return False
    sd.tap_point('welcome_ok', 1.5)
    time.sleep(1.5)
    sd.tap_point('welcome_ok', 1.5)
    run.shot('channel_popup_closed')
    if not sd.enter_channel(USER):
        run.say('never reached the lobby ' + pilot('dump'))
        return False
    run.mark('lobby_stage8')
    time.sleep(3.0)
    run.shot('lobby')
    run.measure_fps('lobby', 4.0)
    return True


def main(argv):
    if len(argv) < 3:
        print(USAGE)
        return 2
    scenario, out = argv[1], argv[2]
    os.makedirs(out, exist_ok=True)
    run = Run(out)
    watcher = Watcher(run)
    run.mark('scenario_start')
    ok = False
    try:
        if scenario == 'boot':
            ok = do_boot(run, watcher)
        else:
            sd = load_driver(run)
            if do_lobby(run, watcher, sd):
                watcher.auto_shot = False
                if scenario == 'lobby':
                    ok = True
                elif scenario == 'walk':
                    ok = bool(sd.walk()) and all(g[2] for g in run.go_results)
                elif scenario == 'refs':
                    watcher.auto_shot = False
                    ok = capture_reference_plan(run, sd)
                elif scenario == 'factory':
                    ok = factory_probe(run, sd)
                elif scenario == 'paint':
                    ok = paint_probe(run, sd)
                elif scenario == 'mission':
                    ok = mission_probe(run, sd)
                elif scenario == 'antenna':
                    ok = antenna_probe(run, sd)
                elif scenario == 'videocheck':
                    ok = videocheck_probe(run, sd)
                elif scenario == 'slotswap':
                    watcher.auto_shot = True
                    log = open(os.path.join(out, 'race_drive.log'), 'w')
                    try:
                        ok = slotswap_probe(run, sd, log)
                    finally:
                        log.close()
                elif scenario == 'race':
                    watcher.auto_shot = True
                    log = open(os.path.join(out, 'race_drive.log'), 'w')
                    try:
                        back = room_race(run, sd, log)
                    finally:
                        log.close()
                    # the server ends the race 20 s after the first finisher so a lap then the room is proof
                    room_again = any(m[0] == 'room_again' for m in run.marks)
                    ok = bool(back) and room_again and (run.finished is not None or run.laps >= 1)
                    if room_again and run.finished is None:
                        run.finished = 'race ended by the server after %d laps of the local car' % run.laps
                    run.shot('after_race')
                else:
                    print(USAGE)
                    return 2
    except Exception as e:
        run.say('scenario error %r' % e)
        run.notes.append('error %r' % e)
    finally:
        watcher.stop = True
        try:
            run.notes.append('last dump ' + pilot('dump', timeout=5))
        except OSError:
            pass
        run.x11_grab(os.path.join(out, 'zz_final_screen.png'))
        run.mark('scenario_end')
        refused = any(n.startswith('login refused') for n in run.notes)
        stuck = any(n.startswith('stuck on stage') for n in run.notes)
        run.save('pass' if ok else ('refused' if refused else ('licence_stuck' if stuck else 'fail')))
    run.say('result %s shots %d in %s' % ('PASS' if ok else ('REFUSED' if refused else 'FAIL'),
                                          sum(1 for s in run.shots if s['file']), out))
    # exit 5 tells the runner the account was busy exit 6 the licence flag needs a database clear
    return 0 if ok else (5 if refused else (6 if stuck else 1))


if __name__ == '__main__':
    sys.exit(main(sys.argv))
