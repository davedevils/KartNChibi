#!/usr/bin/env python3
# the release gate runs every client proof script against the package server and writes the report
import argparse
import csv
import os
import random
import re
import shlex
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CLIENT_EXE = REPO / "release" / "knc_client.exe"
RUN_CWD = REPO
GAME_DIR = os.environ.get("KNC_GAME_DIR", "game")
SCRIPTS_DIR = REPO / "tools" / "client_scripts"
OUT_ROOT = REPO / "reference" / "gate"
DOC_PATH = REPO / "docs" / "client" / "RELEASE_GATE.md"
ENV_FILE = Path(os.environ.get("KNC_SERVER_ENV", "server/.env"))

LOGIN_HOST = "127.0.0.1"
LOGIN_PORT = 50017
WEB_ADMIN = "http://127.0.0.1:8080"
GAME_CONTAINER = "knc-game"
CONTAINERS = ["knc-game", "knc-login", "knc-mariadb"]
DB_CONTAINER = "knc-mariadb"

# the two seed test accounts never touch any other account
ACCOUNT_PASS = {"hltest": "hltestpw", "hltest2": "hltest2pw", "dock1": "dock1pwd", "dock2": "dock2pwd"}
GOLD_TOPUP = 50000

DEFAULT_TIMEOUT = 120
TIMEOUTS = {
    "intro": 90, "login": 60,
    "carcraft": 180, "roomcraft": 180,
    "licence_test": 220, "licence_grade": 260, "licence_pick": 220, "tutorial1": 220,
    "ghost": 260, "race_items": 260, "weather": 260,
    "mission_kind1": 420, "mission_kind1_sample": 420, "mission_kind2_sample": 420,
}

WARN_PATTERN = re.compile(r"warn|error|exception|refused", re.IGNORECASE)
IDENT_RE = re.compile(r"^[A-Za-z0-9_]{3,16}$")


class FixtureError(Exception):
    pass


class Header:
    def __init__(self):
        self.mode = None          # live or state
        self.state_name = None
        self.account = None       # explicit account name or None for default
        self.extra_args = []
        self.env = {}
        self.needs = []


def parse_header(path: Path) -> Header:
    h = Header()
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^#\s*gate:\s*(\S+)(?:\s+(.*))?$", line)
        if not m:
            continue
        key, rest = m.group(1), (m.group(2) or "").strip()
        if key == "live":
            h.mode = "live"
        elif key == "state":
            h.mode = "state"
            h.state_name = rest
        elif key == "account":
            h.account = rest
        elif key == "args":
            h.extra_args = shlex.split(rest)
        elif key == "env":
            if "=" in rest:
                name, val = rest.split("=", 1)
                h.env[name] = val
        elif key == "needs":
            h.needs.append(rest)
    if h.mode is None:
        raise FixtureError(f"{path.name} has no gate header (live or state)")
    return h


def load_env() -> dict:
    out = {}
    for line in ENV_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def docker_ps():
    res = subprocess.run(["docker", "ps", "--format", "{{.Names}}\t{{.Status}}\t{{.Image}}"],
                          capture_output=True, text=True, timeout=30)
    rows = {}
    for line in res.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) >= 3:
            rows[parts[0]] = {"status": parts[1], "image": parts[2]}
    return rows


def check_containers():
    rows = docker_ps()
    problems = []
    for name in CONTAINERS:
        if name not in rows:
            problems.append(f"{name} is not running")
        elif "healthy" not in rows[name]["status"].lower() and "up" not in rows[name]["status"].lower():
            problems.append(f"{name} status is {rows[name]['status']}")
    image = rows.get(GAME_CONTAINER, {}).get("image", "unknown")
    return problems, image


def db_run(sql: str, cred: dict):
    cmd = ["docker", "exec", DB_CONTAINER, "mariadb",
           f"-u{cred['DB_USER']}", f"-p{cred['DB_PASSWORD']}", "-B", "-e", sql, cred["DB_NAME"]]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=30)


def db_query(sql: str, cred: dict):
    res = db_run(sql, cred)
    if res.returncode != 0:
        raise FixtureError(f"db query failed: {res.stderr.strip()}")
    lines = res.stdout.splitlines()
    if not lines:
        return []
    return list(csv.DictReader(lines, delimiter="\t"))


def db_exec(sql: str, cred: dict):
    res = db_run(sql, cred)
    if res.returncode != 0:
        raise FixtureError(f"db exec failed: {res.stderr.strip()} sql={sql}")


def safe_ident(name: str) -> str:
    if not IDENT_RE.match(name):
        raise FixtureError(f"unsafe account name {name!r}")
    return name


def lift_ban(account: str, cred: dict):
    safe_ident(account)
    db_exec(f"UPDATE accounts SET is_banned=0, ban_reason=NULL, ban_expires_at=NULL "
            f"WHERE username='{account}'", cred)
    db_exec(f"DELETE FROM bans WHERE account_id=(SELECT id FROM accounts WHERE username='{account}')", cred)


def top_up_gold(account: str, cred: dict, amount: int = GOLD_TOPUP):
    safe_ident(account)
    db_exec(f"UPDATE characters SET gold={amount}, cash={amount} "
            f"WHERE account_id=(SELECT id FROM accounts WHERE username='{account}')", cred)


def grant_gacha_coin(account: str, cred: dict, base_key: int = 2000):
    safe_ident(account)
    db_exec("INSERT INTO owned_item (character_id, base_key, price_key, period_mode, period_value, "
            "active_flag, in_use_flag) SELECT c.id, {bk}, 0, 0, 0, 1, 0 FROM characters c "
            "JOIN accounts a ON a.id=c.account_id WHERE a.username='{u}' AND NOT EXISTS "
            "(SELECT 1 FROM owned_item oi WHERE oi.character_id=c.id AND oi.base_key={bk} "
            "AND oi.active_flag=1)".format(bk=base_key, u=account), cred)


def grant_pet(account: str, cred: dict, base_key: int = 10):
    safe_ident(account)
    db_exec("INSERT INTO owned_pet (character_id, base_key, equipped_flag, price_key, period_mode, "
            "period_value, active_flag) SELECT c.id, {bk}, 0, 0, 0, 0, 1 FROM characters c "
            "JOIN accounts a ON a.id=c.account_id WHERE a.username='{u}' AND NOT EXISTS "
            "(SELECT 1 FROM owned_pet op WHERE op.character_id=c.id AND op.base_key={bk} "
            "AND op.active_flag=1)".format(bk=base_key, u=account), cred)


def grant_factory_kart(account: str, cred: dict, kart_id: int = 12002):
    safe_ident(account)
    db_exec("INSERT INTO owned_kart (character_id, base_key) SELECT c.id, {k} FROM characters c "
            "JOIN accounts a ON a.id=c.account_id WHERE a.username='{u}' AND NOT EXISTS "
            "(SELECT 1 FROM owned_kart ok WHERE ok.character_id=c.id AND ok.base_key={k})"
            .format(k=kart_id, u=account), cred)


def get_character_id(account: str, cred: dict):
    safe_ident(account)
    rows = db_query(f"SELECT c.id FROM characters c JOIN accounts a ON a.id=c.account_id "
                     f"WHERE a.username='{account}'", cred)
    return int(rows[0]["id"]) if rows else None


def seed_licence_progress(char_id: int, upto: int, cred: dict):
    if upto <= 0:
        return
    values = ",".join(f"({char_id},{k},1,0)" for k in range(upto))
    db_exec(f"REPLACE INTO char_license_progress (char_id, license_key, passed, unknown_08) "
            f"VALUES {values}", cred)


def http_register(username: str, password: str):
    data = urllib.parse.urlencode({"username": username, "password": password,
                                    "confirm": password}).encode("utf-8")
    req = urllib.request.Request(f"{WEB_ADMIN}/register", data=data, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            body = resp.read().decode("utf-8", "replace")
    except urllib.error.URLError as e:
        return False, str(e)
    return ("Account created" in body), body


def make_fresh_name():
    ts = str(int(time.time()))[-7:]
    rnd = f"{random.randint(0, 99):02d}"
    user = f"gf{ts}{rnd}"[:16]
    return user, f"{user}Pw1"


def run_process(cmd, timeout, env=None):
    full_env = os.environ.copy()
    if env:
        full_env.update(env)
    start = datetime.now(timezone.utc)
    # windows can briefly lock a just written exe for a virus scan retry a launch failure once
    launch_attempts = 3
    last_err = None
    for attempt in range(launch_attempts):
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                                  env=full_env, cwd=str(RUN_CWD))
            end = datetime.now(timezone.utc)
            return {"rc": res.returncode, "stdout": res.stdout, "stderr": res.stderr,
                    "timed_out": False, "start": start, "end": end}
        except subprocess.TimeoutExpired as e:
            end = datetime.now(timezone.utc)
            out = e.stdout.decode("utf-8", "replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
            err = e.stderr.decode("utf-8", "replace") if isinstance(e.stderr, bytes) else (e.stderr or "")
            return {"rc": None, "stdout": out, "stderr": err, "timed_out": True, "start": start, "end": end}
        except OSError as e:
            last_err = e
            if attempt < launch_attempts - 1:
                time.sleep(3)
                start = datetime.now(timezone.utc)
                continue
    end = datetime.now(timezone.utc)
    return {"rc": None, "stdout": "", "stderr": f"launch failed after {launch_attempts} tries: {last_err}",
            "timed_out": False, "start": start, "end": end}


def docker_logs_window(container, start, end):
    since = start.strftime("%Y-%m-%dT%H:%M:%S.%fZ")
    until = end.strftime("%Y-%m-%dT%H:%M:%S.%fZ")
    res = subprocess.run(["docker", "logs", container, "--since", since, "--until", until],
                          capture_output=True, text=True, timeout=30)
    return (res.stdout or "") + (res.stderr or "")


def inject_nick(extra_args, nick):
    # forces the create character nickname to the given value so no two fresh runs collide
    args = list(extra_args)
    if "--auto-create-character" in args:
        idx = args.index("--auto-create-character")
        if idx + 1 < len(args):
            args[idx + 1] = nick
        else:
            args.append(nick)
    else:
        args = ["--auto-create-character", nick] + args
    return args


def build_command(header: Header, user, passwd, script_path: Path, out_dir: Path):
    cmd = [str(CLIENT_EXE), "--game", GAME_DIR]
    if header.mode == "state":
        cmd += ["--state", header.state_name]
    else:
        cmd += ["--host", LOGIN_HOST, "--port", str(LOGIN_PORT), "--user", user, "--pass", passwd]
    cmd += ["--no-focus", "--mute"]
    cmd += header.extra_args
    cmd += ["--script", str(script_path)]
    cmd += ["--screenshot", str(out_dir / "out.png")]
    cmd += ["--wire", str(out_dir / "wire.log")]
    return cmd


def prepare_fixtures(header: Header, script_stem: str, out_dir: Path, cred: dict):
    """Returns (user, passwd) for the main run. Raises FixtureError on a hard failure."""
    if header.mode == "state":
        return None, None
    account = header.account or "hltest"
    if "fresh" in header.needs:
        user, passwd = make_fresh_name()
        ok, body = http_register(user, passwd)
        if not ok:
            raise FixtureError(f"register failed for {user}: {body[:200]}")
        # the account name doubles as a unique nickname so MSG ALREADY REGIST never fires
        nick = user
        lic_n = 0
        if "KNC_AUTO_LICENCE" in header.env:
            try:
                lic_n = int(header.env["KNC_AUTO_LICENCE"])
            except ValueError:
                lic_n = 0
        if lic_n > 0:
            prep_dir = out_dir / "prep"
            prep_dir.mkdir(parents=True, exist_ok=True)
            prep_cmd = [str(CLIENT_EXE), "--game", GAME_DIR, "--host", LOGIN_HOST, "--port", str(LOGIN_PORT),
                        "--user", user, "--pass", passwd, "--no-focus", "--mute",
                        "--auto-create-character", nick, "--stop-at", "licence",
                        "--screenshot", str(prep_dir / "prep.png"), "--wire", str(prep_dir / "wire.log")]
            res = run_process(prep_cmd, timeout=90)
            (prep_dir / "stdout.log").write_text(res["stdout"] + res["stderr"], encoding="utf-8")
            char_id = get_character_id(user, cred)
            if char_id is None:
                raise FixtureError(f"fresh account {user} has no character row after the prep run")
            seed_licence_progress(char_id, lic_n, cred)
        else:
            header.extra_args = inject_nick(header.extra_args, nick)
        return user, passwd
    if account not in ACCOUNT_PASS:
        raise FixtureError(f"account '{account}' is not a known test account, refusing to touch it")
    lift_ban(account, cred)
    if "gold" in header.needs:
        top_up_gold(account, cred)
    if "coin" in header.needs:
        grant_gacha_coin(account, cred)
    if "pet" in header.needs:
        grant_pet(account, cred)
    if "factory kart" in header.needs:
        grant_factory_kart(account, cred)
    return account, ACCOUNT_PASS[account]


def parse_expects(stdout: str):
    fails = []
    all_expects = []
    for line in stdout.splitlines():
        m = re.match(r"\[script\] expect (\S+) (PASS|FAIL) top (\S*)", line)
        if m:
            want, res, top = m.group(1), m.group(2), m.group(3)
            all_expects.append((want, res, top))
            if res == "FAIL":
                fails.append(f"wanted {want} got {top}")
    return all_expects, fails


def run_one(name: str, path: Path, out_root: Path, cred: dict, use_fixtures: bool):
    result = {"name": name, "result": "FAIL", "fails": [], "warnings": [], "note": "", "dir": ""}
    out_dir = out_root / name
    out_dir.mkdir(parents=True, exist_ok=True)
    result["dir"] = str(out_dir.relative_to(REPO))
    try:
        header = parse_header(path)
    except FixtureError as e:
        result["note"] = f"header error: {e}"
        return result

    if header.mode == "live" and "fresh" in header.needs and not use_fixtures:
        result["result"] = "SKIP"
        result["note"] = "needs a fresh account, rerun with fixtures on"
        return result

    try:
        if use_fixtures:
            user, passwd = prepare_fixtures(header, name, out_dir, cred)
        else:
            user = header.account or "hltest"
            passwd = ACCOUNT_PASS.get(user, "")
    except FixtureError as e:
        result["result"] = "FAIL"
        result["note"] = f"fixture prep failed: {e}"
        return result

    cmd = build_command(header, user, passwd, path, out_dir)
    (out_dir / "cmd.txt").write_text(" ".join(cmd), encoding="utf-8")
    timeout = TIMEOUTS.get(name, DEFAULT_TIMEOUT)
    run_env = dict(header.env) if header.env else None
    res = run_process(cmd, timeout, env=run_env)
    (out_dir / "stdout.log").write_text(res["stdout"] + "\n---stderr---\n" + res["stderr"], encoding="utf-8")

    if res["timed_out"]:
        result["result"] = "FAIL"
        result["note"] = f"timed out after {timeout}s"
    elif res["rc"] == 0:
        result["result"] = "PASS"
    elif res["rc"] == 1:
        result["result"] = "FAIL"
    elif res["rc"] is None:
        result["result"] = "FAIL"
        result["note"] = f"could not launch the client: {res['stderr'][:200]}"
    else:
        result["result"] = "FAIL"
        result["note"] = f"exit code {res['rc']}"

    _, fails = parse_expects(res["stdout"])
    result["fails"] = fails

    try:
        log = docker_logs_window(GAME_CONTAINER, res["start"], res["end"])
        warn_lines = [l for l in log.splitlines() if WARN_PATTERN.search(l)]
        (out_dir / "server_warnings.log").write_text("\n".join(warn_lines), encoding="utf-8")
        result["warnings"] = warn_lines
    except Exception as e:
        result["warnings"] = [f"could not read server log: {e}"]

    return result


def discover_scripts(only=None):
    scripts = sorted(SCRIPTS_DIR.glob("*.txt"))
    if only:
        wanted = set()
        for token in only:
            for part in re.split(r"[,\s]+", token.strip()):
                if part:
                    wanted.add(part.replace(".txt", ""))
        scripts = [s for s in scripts if s.stem in wanted]
    return scripts


def load_existing_real_bugs():
    if not DOC_PATH.exists():
        return None
    text = DOC_PATH.read_text(encoding="utf-8")
    m = re.search(r"(^## Real bugs.*)$", text, re.MULTILINE | re.DOTALL)
    return m.group(1) if m else None


def load_existing_table_rows():
    # a rerun with --only must not erase the rows of scripts it did not touch this time
    if not DOC_PATH.exists():
        return {}
    text = DOC_PATH.read_text(encoding="utf-8")
    rows = {}
    for line in text.splitlines():
        m = re.match(r"^\|\s*([A-Za-z0-9_]+)\s*\|.*\|$", line)
        if m and m.group(1) not in ("Script",):
            rows[m.group(1)] = line
    return rows


def format_row(r):
    fails = "; ".join(r["fails"]) if r["fails"] else ("-" if r["result"] != "SKIP" else r["note"])
    warns = str(len(r["warnings"])) if r["warnings"] else "0"
    note = f" ({r['note']})" if r["note"] and r["result"] != "SKIP" else ""
    return f"| {r['name']} | {r['result']}{note} | {fails} | {warns} | `{r['dir']}` |"


def write_doc(date_str, image, results):
    lines = []
    lines.append("# Release gate")
    lines.append("")
    lines.append(f"Ran {date_str} against `{image}` ({', '.join(CONTAINERS)} checked healthy first).")
    lines.append("")
    lines.append("Run with `python tools/release_gate.py`, options `--only <names>`, "
                  "`--keep-going`, `--no-fixtures`. A `--only` run merges its rows into the table, "
                  "it does not drop the rows of scripts it left alone.")
    lines.append("")
    lines.append("| Script | Result | Failing expect | Server warnings | Shots |")
    lines.append("|---|---|---|---|---|")
    merged = load_existing_table_rows()
    for r in results:
        merged[r["name"]] = format_row(r)
    for name in sorted(merged):
        lines.append(merged[name])
    lines.append("")

    old_bugs = load_existing_real_bugs()
    if old_bugs:
        lines.append(old_bugs.rstrip())
    else:
        lines.append("## Real bugs")
        lines.append("")
        lines.append("None recorded yet, fill this in by hand after triage.")
    lines.append("")

    DOC_PATH.parent.mkdir(parents=True, exist_ok=True)
    DOC_PATH.write_text("\n".join(lines), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description="run every client proof script against the package server")
    ap.add_argument("--only", nargs="*", help="run only these script names")
    ap.add_argument("--keep-going", action="store_true",
                     help="keep running the batch even if a fixture step fails hard")
    ap.add_argument("--no-fixtures", action="store_true", help="skip ban lift gold top up and fresh accounts")
    ap.add_argument("--exe", help="client exe to run, default release/knc_client.exe")
    ap.add_argument("--cwd", help="working dir for the client process, default the repo root")
    args = ap.parse_args()

    global CLIENT_EXE, RUN_CWD
    if args.exe:
        CLIENT_EXE = Path(args.exe)
    if args.cwd:
        RUN_CWD = Path(args.cwd)

    os.chdir(REPO)

    if not CLIENT_EXE.exists():
        print(f"[gate] missing {CLIENT_EXE}", file=sys.stderr)
        return 2

    problems, image = check_containers()
    if problems:
        for p in problems:
            print(f"[gate] {p}", file=sys.stderr)
        return 2
    print(f"[gate] containers healthy, game image {image}")

    cred = load_env() if not args.no_fixtures else {}

    scripts = discover_scripts(args.only)
    if not scripts:
        print("[gate] no scripts matched", file=sys.stderr)
        return 2

    date_str = datetime.now().strftime("%Y-%m-%d")
    out_root = OUT_ROOT / date_str
    out_root.mkdir(parents=True, exist_ok=True)

    results = []
    for path in scripts:
        name = path.stem
        print(f"[gate] running {name}")
        try:
            r = run_one(name, path, out_root, cred, use_fixtures=not args.no_fixtures)
        except Exception as e:
            r = {"name": name, "result": "FAIL", "fails": [], "warnings": [],
                 "note": f"gate crashed: {e}", "dir": ""}
            if not args.keep_going:
                results.append(r)
                print(f"[gate] {name}: FAIL ({r['note']}), stopping, pass --keep-going to continue")
                write_doc(date_str, image, results)
                return 1
        results.append(r)
        print(f"[gate] {name}: {r['result']}" + (f" ({r['note']})" if r["note"] else ""))

    write_doc(date_str, image, results)
    failed = [r for r in results if r["result"] == "FAIL"]
    print(f"[gate] {len(results)} scripts, {len(failed)} failed, wrote {DOC_PATH.relative_to(REPO)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
