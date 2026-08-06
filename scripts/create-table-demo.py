#!/usr/bin/env python3
# Table alignment demo script.
"""Generate the markdown-table alignment demo GIF for Scriba.

Types the header row of a markdown table — the editor auto-completes the
separator plus a data row and pads every column to equal width — then inserts
right / left / center alignment markers into the separator with arrow keys, and
fills a few rows of differing cell lengths to show the equal-width padding and
the per-column alignment.

Usage: python3 scripts/create-table-demo.py
"""

import os
import shutil
import time

from demo_helper import DemoScribe, PROJECT_DIR, XDG_CONFIG, launch_under_xvfb

launch_under_xvfb()

OUTPUT = PROJECT_DIR / "docs" / "images" / "table-demo.gif"


class TableDemo(DemoScribe):
    def _setup_env(self):
        # Base class enables emoji autocomplete; here we want it off so that
        # typing ':' never pops an emoji list while editing the separator.
        # The language is pinned to en_GB AND the grammar dialect to British so
        # that "Centre" (vs US "Center") in the header is not flagged by the
        # spell checker during the demo.  (dialect alone defaults the base
        # dictionary to en_GB when dictionaryLanguage is empty, but setting both
        # is explicit.)
        shutil.rmtree(str(XDG_CONFIG), ignore_errors=True)
        (XDG_CONFIG / "scriba").mkdir(parents=True)
        cfg = XDG_CONFIG / "scriba" / "scriba.conf"
        cfg.write_text(
            "[General]\nemojiMode=color\n"
            "activeCssFile=:/themes/catppuccin-mocha.css\n"
            "emojiAutoComplete=false\n"
            "dictionaryLanguage=en_GB\n"
            "grammarDialect=British\n"
        )
        os.environ["XDG_CONFIG_HOME"] = str(XDG_CONFIG)

    def _ins(self, pos):
        """Home then `pos` Right-presses, then type a colon.  Because we edit the
        separator right-to-left (rightmost column first), an insertion never
        shifts positions to its left, so the absolute offsets stay valid."""
        self.press("Home")
        for _ in range(pos):
            self.press("Right")
        time.sleep(0.15)
        self.press_char(":")
        self.capture()
        time.sleep(0.15)

    def _type_row(self, cells):
        for i, cell in enumerate(cells):
            self.type_chars(cell)
            self.press("Tab")
            self.capture()
            self.capture()

    def run(self):
        self.start_scriba()

        self.header("Markdown Tables")

        # Brief explanatory line below the header: the source is padded so
        # every column is the same width, and each keeps its alignment.
        self.type_str("Source is padded; columns keep their alignment")
        self.capture()
        self.press("Return"); self.capture()
        self.press("Return"); self.capture()

        # Type the header row; each character appears as it is typed.
        self.type_chars("| Right | Left | Centre |")
        # Return auto-completes the separator + an empty data row and pads.
        self.press("Return")
        self.pause(9)
        self.capture()

        # Add alignment colons to the separator.  After autocomplete the
        # separator is padded to the header cells:
        #   Right(5) Left(4) Centre(6)  ->  |-------|------|--------|
        # Cursor is in the empty data row; Home then Up lands on the separator.
        self.press("Home"); self.capture()
        self.press("Up"); self.capture()
        self.capture()

        # Desired separator (right / left / center):
        #   |-------:|:------|:--------:|
        # Insert right-to-left so an earlier insert never shifts a later one.
        # Padded separator cell boundaries (0-based colon insert positions):
        #   col3 (Centre)  :--------:  -> first ':' at 16, last at 24
        #   col2 (Left)    :------       -> first ':' at 9
        #   col1 (Right)   -------:      -> last  ':' at 8
        self._ins(24)   # col3 trailing ':'
        self._ins(16)   # col3 leading  ':'
        self._ins(9)    # col2 leading  ':'
        self._ins(8)    # col1 trailing ':'

        # Back down to the first data row's first cell.
        self.press("Down"); self.press("Home"); self.capture()
        self.press("Right"); self.press("Right"); self.capture()

        # Fill cells of differing lengths; Tab advances/nests the row, so evenly
        # writing 3 rows here yields a 3-row table.
        self._type_row(["Apple", "red", "10"])
        self._type_row(["Watermelon", "green", "145"])
        self._type_row(["Pear", "blue", "11"])

        # Return on the trailing empty row exits the table; the editor then
        # re-pads every column to the widest cell across all rows.
        self.press("Return"); self.pause(8)
        self.press("Return"); self.pause(8)
        self.capture()

        self.finish()


if __name__ == "__main__":
    TableDemo(OUTPUT).run()