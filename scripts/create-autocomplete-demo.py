#!/usr/bin/env python3
# Autocomplete demo script.
"""Generate autocomplete demo GIF for Scriba.

Usage: python3 scripts/create-autocomplete-demo.py
"""

import time

from demo_helper import launch_under_xvfb, DemoScribe, PROJECT_DIR

launch_under_xvfb()

OUTPUT = PROJECT_DIR / "docs" / "images" / "autocomplete-demo.gif"


class DemoScribeApp(DemoScribe):
    def _scene_file_autocomplete(self):
        self.header("Local Filenames")
        self.press("Return"); self.capture()

        self.type_with_meta(["exclam", "bracketleft", "bracketright", "parenleft"])
        self.type_with_meta(["r", "e", "s", "o"])
        time.sleep(0.2)
        self.capture()

        self.press("Return"); self.pause(9)

        self.type_with_meta(["i", "c"])
        time.sleep(0.2)
        self.capture()

        self.press("Return"); self.pause(9)

        self.type_with_meta(["s", "c", "r"])
        time.sleep(0.3)
        self.press("Down"); self.capture()
        time.sleep(0.2)
        self.capture()

        self.press("Return"); self.pause(9)

        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

    def _scene_emoji_autocomplete(self):
        self.header("Emojis - Type ':' to start")

        self.type_with_meta(["colon", "s", "m", "i"])
        time.sleep(0.2)
        self.capture()

        self.press("Down"); self.capture(); self.capture()
        self.press("Down"); self.capture(); self.capture()

        self.press("Return")
        self.pause(12)

        self.press("space"); self.capture()

        self.type_with_meta(["colon", "r", "o", "c", "k"])
        time.sleep(0.2)
        self.capture()

        self.press("Down"); self.capture(); self.capture()
        self.pause(12)
        self.press("Return")
        self.pause(15)
        self.capture()

        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

    def _scene_code_block(self):
        self.header("Code Blocks")

        text = 'Type ``` and the first letters of a language, <enter> to accept'
        self.type_str(text)
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        self.type_with_meta(["grave", "grave", "grave", "p", "y"])
        time.sleep(0.3)
        self.capture()

        self.press("Return"); self.pause(9)
        self.press("Return"); self.capture()
        self.type_str('print("Hello, World!")')
        self.capture()
        self.press("Return"); self.capture()
        self.type_str('```')
        self.press("Return"); self.pause(6)
        self.press("Return"); self.pause(30)

    def run(self):
        self.start_scriba()
        self._scene_file_autocomplete()
        self._scene_emoji_autocomplete()
        self._scene_code_block()
        self.finish()


if __name__ == "__main__":
    DemoScribeApp(OUTPUT).run()