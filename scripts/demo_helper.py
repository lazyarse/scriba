#!/usr/bin/env python3
"""Shared Xvfb / xdotool / mss infrastructure for Scriba demo scripts.

The demo scripts (create-autocomplete-demo.py, create-table-demo.py) launch
Scriba under xvfb-run, drive it with xdotool, capture frames with mss and
assemble a GIF.  All of that shelling-out lives here so the per-feature demo
scripts only contain the keystroke choreography.
"""

import os, sys, time, subprocess, shutil
from pathlib import Path
from PIL import Image
import mss

_XVFB_ENV = "_SCRIBA_DEMO_IN_XVFB"

PROJECT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_DIR / "build"
XDG_CONFIG = Path("/tmp/scriba-demo-config")
DEMO_FILE = PROJECT_DIR / ".demo-content.md"

_DEFAULT_REGION = {"top": 0, "left": 0, "width": 1000, "height": 400}


def launch_under_xvfb():
    """Re-exec the current script under xvfb-run unless already inside."""
    if _XVFB_ENV in os.environ:
        return
    print("Launching under xvfb-run...")
    cmd = [
        "xvfb-run", "-a",
        "--server-args=-screen 0 1000x450x24",
        sys.executable, sys.argv[0],
    ] + sys.argv[1:]
    env = {**os.environ, _XVFB_ENV: "1"}
    try:
        proc = subprocess.run(cmd, env=env)
        sys.exit(proc.returncode)
    except FileNotFoundError:
        print("xvfb-run not found. Install it: sudo apt install xvfb")
        sys.exit(1)


class DemoScribe:
    """Drives a Scriba window under Xvfb, capturing frames as a GIF.

    Keys are sent with XTEST (`xdotool windowfocus WID` then `xdotool key`
    without `--window`) because XSendEvent does not trigger shortcuts in Qt.
    """

    def __init__(self, output_path, region=None):
        self.output = Path(output_path)
        self.region = region or dict(_DEFAULT_REGION)
        self.frames = []
        self.wid = None
        self.scriba_proc = None
        self._sct = None
        self._setup_env()

    def _setup_env(self):
        shutil.rmtree(str(XDG_CONFIG), ignore_errors=True)
        (XDG_CONFIG / "scriba").mkdir(parents=True)
        cfg = XDG_CONFIG / "scriba" / "scriba.conf"
        cfg.write_text(
            "[General]\nemojiMode=color\n"
            "activeCssFile=:/themes/catppuccin-mocha.css\n"
        )
        os.environ["XDG_CONFIG_HOME"] = str(XDG_CONFIG)

    def start_scriba(self, file_path=None):
        target = file_path if file_path is not None else DEMO_FILE
        Path(target).touch()
        self.scriba_proc = subprocess.Popen(
            [str(BUILD_DIR / "scriba"), str(target)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        time.sleep(4)
        deadline = time.time() + 10
        self.wid = ""
        while time.time() < deadline:
            out = subprocess.run(
                ["xdotool", "search", "--onlyvisible", "--name", "Scriba"],
                capture_output=True, text=True,
            )
            wids = out.stdout.strip().split("\n")
            if wids and wids[0]:
                self.wid = wids[0]
                break
            time.sleep(0.1)
        if not self.wid:
            raise RuntimeError("Scriba window not found within 10s timeout")
        w, h = self.region["width"], self.region["height"]
        subprocess.run(["xdotool", "windowsize", self.wid, str(w), str(h)])
        subprocess.run(["xdotool", "windowmove", self.wid, "0", "0"])
        time.sleep(0.15)
        subprocess.run(["xdotool", "mousemove", "--window", self.wid,
                        str(w // 2), str(h // 2)])
        subprocess.run(["xdotool", "windowfocus", self.wid])
        subprocess.run(["xdotool", "click", "1"])
        time.sleep(0.05)
        self._sct = mss.MSS()

    @staticmethod
    def _xkey(ch):
        return {" ": "space", ",": "comma", ".": "period",
                "'": "apostrophe", "!": "exclam", ":": "colon",
                "(": "parenleft", ")": "parenright",
                "[": "bracketleft", "]": "bracketright",
                "#": "numbersign", "-": "minus",
                "|": "bar", "/": "slash",
                "`": "grave"}.get(ch, ch)

    def press(self, key):
        subprocess.run(["xdotool", "windowfocus", self.wid], capture_output=True)
        subprocess.run(["xdotool", "key", key], capture_output=True)

    def press_into(self, wid, key):
        subprocess.run(["xdotool", "windowfocus", wid], capture_output=True)
        time.sleep(0.02)
        subprocess.run(["xdotool", "key", key], capture_output=True)

    def capture(self, settle=0.12):
        time.sleep(settle)
        raw = self._sct.grab(self.region)
        img = Image.frombytes("RGB", raw.size, raw.rgb)
        self.frames.append(img)

    def press_char(self, ch):
        self.press(self._xkey(ch))

    def type_str(self, text):
        subprocess.run(
            ["xdotool", "type", "--window", self.wid, "--", text],
            capture_output=True,
        )
        time.sleep(0.04)

    def type_chars(self, text):
        """Type text one character at a time, capturing after each, so the
        markdown visibly appears as it is typed in the GIF."""
        for ch in text:
            self.press_char(ch)
            self.capture()

    def type_with_meta(self, keys):
        """Press a sequence of already-mapped key names, capturing each."""
        for key in keys:
            self.press(key)
            self.capture()

    def header(self, text):
        self.type_str(f"## {text}")
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

    def pause(self, count=3):
        for _ in range(count):
            self.capture()

    def assemble_gif(self, duration=100):
        self.output.parent.mkdir(parents=True, exist_ok=True)
        self.frames[0].save(
            self.output,
            save_all=True,
            append_images=self.frames[1:],
            loop=0,
            duration=duration,
        )
        print(f"Created {self.output} ({len(self.frames)} frames)")

    def wait_for_dialog(self, name, timeout=3.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            out = subprocess.run(
                ["xdotool", "search", "--name", name],
                capture_output=True, text=True,
            )
            wids = out.stdout.strip().split("\n")
            if wids and wids[0]:
                return wids[-1]
            time.sleep(0.02)
        return None

    def finish(self):
        self.scriba_proc.kill()
        self.scriba_proc.wait()
        self._sct.close()
        self.assemble_gif()
        DEMO_FILE.unlink(missing_ok=True)