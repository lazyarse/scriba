#!/usr/bin/env python3
# Autocomplete demo script.
"""Generate autocomplete demo GIF for Scriba.

Usage: python3 scripts/create-autocomplete-demo.py
"""

import os, sys, time, subprocess, shutil

_XVFB_ENV = "_SCRIBA_DEMO_IN_XVFB"
if _XVFB_ENV not in os.environ:
    print("Launching under xvfb-run...")
    cmd = [
        "xvfb-run", "-a",
        "--server-args=-screen 0 800x450x24",
        sys.executable, __file__,
    ] + sys.argv[1:]
    env = {**os.environ, _XVFB_ENV: "1"}
    try:
        proc = subprocess.run(cmd, env=env)
        sys.exit(proc.returncode)
    except FileNotFoundError:
        print("xvfb-run not found. Install it: sudo apt install xvfb")
        sys.exit(1)

from pathlib import Path
from PIL import Image
import mss

PROJECT_DIR = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_DIR / "build"
OUTPUT = PROJECT_DIR / "docs" / "images" / "autocomplete-demo.gif"
XDG_CONFIG = Path("/tmp/scriba-demo-config")
DEMO_FILE = PROJECT_DIR / ".demo-content.md"
REGION = {"top": 0, "left": 0, "width": 800, "height": 400}


class DemoScribe:
    def __init__(self):
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

    def start_scriba(self):
        DEMO_FILE.touch()
        self.scriba_proc = subprocess.Popen(
            [str(BUILD_DIR / "scriba"), str(DEMO_FILE)],
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
        subprocess.run(["xdotool", "windowsize", self.wid, "800", "400"])
        subprocess.run(["xdotool", "windowmove", self.wid, "0", "0"])
        time.sleep(0.15)
        subprocess.run(["xdotool", "mousemove", "--window", self.wid, "100", "100"])
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
                "|": "bar", "/": "slash"}.get(ch, ch)

    def press(self, key):
        subprocess.run(
            ["xdotool", "key", "--window", self.wid, key],
            capture_output=True,
        )

    def press_into(self, wid, key):
        subprocess.run(
            ["xdotool", "windowfocus", wid],
            capture_output=True,
        )
        time.sleep(0.02)
        subprocess.run(
            ["xdotool", "key", "--window", wid, key],
            capture_output=True,
        )

    def capture(self):
        time.sleep(0.12)
        raw = self._sct.grab(REGION)
        img = Image.frombytes("RGB", raw.size, raw.rgb)
        self.frames.append(img)

    def type_str(self, text):
        subprocess.run(
            ["xdotool", "type", "--window", self.wid, "--", text],
            capture_output=True,
        )
        time.sleep(0.04)

    def type_2per(self, text):
        for i in range(0, len(text), 8):
            chunk = text[i:i+8]
            for ch in chunk:
                self.press(self._xkey(ch))
            self.capture()

    def header(self, text):
        self.type_str(f"## {text}")
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

    def pause(self, count=3):
        for _ in range(count):
            self.capture()

    def assemble_gif(self):
        self.frames[0].save(
            OUTPUT,
            save_all=True,
            append_images=self.frames[1:],
            loop=0,
            duration=300,
        )
        print(f"Created {OUTPUT} ({len(self.frames)} frames)")

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

    def run(self):
        self.start_scriba()

        # ── Scene 1: File autocomplete ──
        self.type_str("## Autocomplete")
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        self.header("Files")

        text = "Local filenames, <Enter> to accept"
        self.type_str(text)
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        for ch in ["exclam", "bracketleft", "bracketright", "parenleft"]:
            self.press(ch); self.capture()

        for ch in ["r", "e", "s", "o"]:
            self.press(ch); self.capture()
        time.sleep(0.2)
        self.capture()

        self.press("Return"); self.pause(3)

        for ch in ["i", "c"]:
            self.press(ch); self.capture()
        time.sleep(0.2)
        self.capture()

        self.press("Return"); self.pause(3)

        for ch in ["s", "c", "r"]:
            self.press(ch); self.capture()
        self.press("i")
        time.sleep(0.2)
        self.capture()

        self.press("Return"); self.pause(3)

        self.press("Left"); self.capture()
        for ch in ["numbersign", "8", "0", "x"]:
            self.press(ch); self.capture()
        self.press("Right"); self.capture()

        self.press("Return")
        self.press("Return")

        # ── Scene 2: Emoji autocomplete ──
        self.header("Emojis")

        text = '":" to start'
        self.type_str(text)
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        for ch in ["colon", "s", "m"]:
            self.press(ch); self.capture()
        self.press("i")
        time.sleep(0.2)
        self.capture()

        self.press("Down"); self.capture(); self.capture()
        self.press("Down"); self.capture(); self.capture()

        self.press("Return"); self.pause(3)

        self.press("Return")
        self.press("Return")

        # ── Scene 3: Table autocomplete ──
        self.header("Markdown Tables")

        text = ("<enter> = new row, <tab> = next cell "
                "<shift+tab> = previous, <enter enter> = end table")
        self.type_str(text)
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        for ch in ["bar", "h", "e", "a", "d", "e", "r", "1", "bar"]:
            self.press(ch)
        self.capture()
        for ch in ["h", "e", "a", "d", "e", "r", "2", "bar"]:
            self.press(ch)
        self.capture()
        for ch in ["h", "e", "a", "d", "e", "r", "3", "bar"]:
            self.press(ch)
        self.capture()

        self.press("Return"); self.pause(3)

        for ch in ["c", "e", "l", "l", "1"]:
            self.press(ch)
        self.capture()
        self.press("Tab"); self.capture(); self.capture()
        for ch in ["c", "e", "l", "l", "2"]:
            self.press(ch)
        self.capture()
        self.press("Tab"); self.capture(); self.capture()
        for ch in ["c", "e", "l", "l", "3"]:
            self.press(ch)
        self.capture()
        self.press("Return"); self.pause(3)

        for ch in ["c", "e", "l", "l", "4"]:
            self.press(ch)
        self.capture()
        self.press("Tab"); self.capture(); self.capture()
        for ch in ["c", "e", "l", "l", "5"]:
            self.press(ch)
        self.capture()
        self.press("Tab"); self.capture(); self.capture()
        for ch in ["c", "e", "l", "l", "6"]:
            self.press(ch)
        self.capture()
        self.capture()

        self.press("Shift+Tab"); self.capture(); self.capture()

        for ch in ["p", "r", "e", "v"]:
            self.press(ch)
        self.capture()

        self.press("Return"); self.pause(3)
        self.capture()
        self.press("Return"); self.pause(2)
        self.capture()

        subprocess.run(["xdotool", "windowfocus", self.wid], capture_output=True)
        time.sleep(0.1)
        self.capture()

        # ── Scene 4: HTML table via Ctrl+T ──
        self.header("HTML Tables")

        text = "Use the HTML Table generator - markdown tables forces a header row"
        self.type_str(text)
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        self.press("ctrl+t")
        time.sleep(0.5)
        self.pause(4)
        dlg_wid = self.wait_for_dialog("Insert Table", timeout=5.0)
        if not dlg_wid:
            raise RuntimeError("Insert Table dialog not found")
        self.pause(2)

        self.press_into(dlg_wid, "2"); self.capture()
        self.press_into(dlg_wid, "Tab"); self.capture()
        self.press_into(dlg_wid, "space"); self.capture()
        self.pause(2)

        self.press_into(dlg_wid, "alt+i")
        time.sleep(0.1)
        self.capture()

        for ch in ["f", "o", "o"]:
            self.press(ch); self.capture()
        self.press("Tab"); self.capture(); self.capture()
        for ch in ["b", "a", "r"]:
            self.press(ch); self.capture()
        self.press("Return"); self.pause(3)

        for ch in ["f", "o", "o", "2"]:
            self.press(ch); self.capture()
        self.press("Tab"); self.capture(); self.capture()
        for ch in ["b", "a", "r", "2"]:
            self.press(ch); self.capture()
        self.press("Return"); self.pause(3)

        self.press("Return"); self.pause(3)

        self.scriba_proc.kill()
        self.scriba_proc.wait()
        self._sct.close()
        self.assemble_gif()
        DEMO_FILE.unlink(missing_ok=True)


if __name__ == "__main__":
    DemoScribe().run()
