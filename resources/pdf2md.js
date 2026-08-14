// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// PDF structure -> Markdown heuristics.
//
// Input: an object { pages: [...] } where each page is
//   { items: [ { str, tx, ty, width, height, fontName } ], height: number,
//     fontStyles: { <fontName>: { name, isMonospace, black, italic } } }
// tx/ty are pdf.js transform[4]/[5] (PDF user space; y increases upward).
// fontStyles resolve each internal fontName to the real PostScript face name
// pulled from the page's commonObjs (see src/src/io/PdfImporter.cpp).
//
// The algorithm set is a port of pdf-to-markdown (github.com/iamarunbrahma/
// pdf-to-markdown, MIT) from pdfplumber geometry onto pdf.js geometry.
(function () {
  'use strict';

  var BULLET_RE = /^[\u2022\u00b7\u25aa\u25e6\u25cf\u25cb\u2043\u2219\-]\s+/;
  var OL_RE = /^\d{1,3}[.)]\s/;
  var MONO_RE = /(?:\bCourier\b|\bMono\b|Typewriter|Consolas|Menlo|Monaco|LiberationMono|DejaVuSansMono|UbuntuMono|monospace)/i;
  var PAGE_FRACTION = 0.08; // top/bottom band considered "running header/footer"

  function clamp(n, lo, hi) { return Math.min(hi, Math.max(lo, n)); }

  // ---- Line assembly -------------------------------------------------------
  // Group items sharing a y-baseline into lines, sorted top-to-bottom then
  // left-to-right. Attaches each chunk's resolved font style.
  function assembleLines(page) {
    var fontStyles = page.fontStyles || {};
    var lines = [];
    var cur = null;
    var forceBreak = false;
    for (var i = 0; i < page.items.length; i++) {
      var it = page.items[i];
      if (!it || typeof it.str !== 'string') continue;
      // pdf.js emits empty hasEOL items as line separators at reading order.
      if (it.str === '') {
        if (it.hasEOL === true) forceBreak = true;
        continue;
      }
      // Filler whitespace besides real text (width-only layout gaps).
      if (!it.str.trim() && (!it.height || it.height <= 0)) continue;
      var sameLine;
      if (!cur) {
        sameLine = false;
      } else if (forceBreak) {
        sameLine = false; // previous line ended (hasEOL)
      } else {
        var adj = Math.max(0.4, Math.min(it.height, cur.h) * 0.5 || 3);
        sameLine = Math.abs(it.ty - cur.y) <= adj && it.height / (cur.h || it.height) < 1.9;
      }
      if (!sameLine) {
        cur = { y: it.ty, chunks: [] };
        lines.push(cur);
      }
      cur.chunks.push(it);
      if (it.height > (cur.h || 0)) cur.h = it.height;
      forceBreak = it.hasEOL === true;
    }

    return lines.map(function (ln) {
      ln.chunks.sort(function (a, b) { return a.tx - b.tx; });
      var text = '';
      var x0 = Infinity, x1 = -Infinity, h = 0;
      for (var k = 0; k < ln.chunks.length; k++) {
        var c = ln.chunks[k];
        if (text && !/[\s]$/.test(text) && !/^[\s]/.test(c.str)) text += ' ';
        text += c.str;
        ln.style = ln.style || {};
        var st = fontStyles[c.fontName];
        if (st) {
          c.style = st;
          if (st.italic === true || /Italic|Oblique/i.test(st.name || '')) ln.style.italic = true;
          if (st.black === true || /\bBold\b|\bBlack\b|\bHeavy\b/i.test(st.name || '')) ln.style.bold = true;
          if (st.isMonospace === true || MONO_RE.test(st.name || '')) ln.style.mono = true;
        }
        if (c.tx < x0) x0 = c.tx;
        if (c.tx + (c.width || 0) > x1) x1 = c.tx + (c.width || 0);
        if (c.height > h) h = c.height;
      }
      ln.text = text.replace(/\u00a0/g, ' ').replace(/[ \t]+/g, ' ').trim();
      ln.x0 = x0; ln.x1 = x1 > x0 ? x1 : x0 + 1; ln.height = h > 0 ? h : (ln.chunks.length ? ln.chunks[0].height : 10);
      return ln;
    }).filter(function (ln) { return ln.text.length > 0; });
  }

  // ---- Document-wide font-size stats --------------------------------------
  function sizeStats(lines) {
    var freq = {}, order = [];
    for (var i = 0; i < lines.length; i++) {
      var s = Math.round(lines[i].height * 10) / 10;
      if (s <= 0) continue;
      if (!freq[s]) { freq[s] = 0; order.push(s); }
      freq[s]++;
    }
    var body = null, best = -1;
    for (var k = 0; k < order.length; k++) {
      if (freq[order[k]] > best) { best = freq[order[k]]; body = order[k]; }
    }
    if (!body) return { body: 11, levels: {} };
    var sizes = order.slice().sort(function (a, b) { return b - a; });
    var levels = {};
    for (var l = 0; l < sizes.length; l++) {
      var s = sizes[l];
      if (s <= body * 1.1) break;
      levels[s] = clamp(l + 1, 1, 4);
    }
    return { body: body, levels: levels };
  }

  // ---- Table detection (x-column clustering) ------------------------------
  // A table: >= 2 consecutive lines with >= 2 columns each, whose chunk
  // x-starts align within tolerance and whose y spacing is within one line.
  function detectTables(lines, isBlocked) {
    var n = lines.length, blocks = [];
    var i = 0;
    while (i < n) {
      if (isBlocked(i)) { i++; continue; }
      var j = i + 1;
      while (j < n && !isBlocked(j) && sameColumns(lines[i], lines[j])
             && Math.abs(lines[j].y - lines[j - 1].y) < Math.max(lines[j - 1].height * 2.5, 16)) {
        j++;
      }
      if (j - i >= 2) { blocks.push([i, j]); i = j; }
      else i++;
    }
    return blocks;
  }
  function sameColumns(a, b) {
    if (a.chunks.length < 2 || a.chunks.length !== b.chunks.length) return false;
    if (isListLine(a.text) || isListLine(b.text)) return false;
    for (var k = 0; k < a.chunks.length; k++) {
      if (Math.abs(a.chunks[k].tx - b.chunks[k].tx) > 3) return false;
    }
    return true;
  }

  // ---- List / blockquote classification -----------------------------------
  function isListLine(t) { return OL_RE.test(t) || BULLET_RE.test(t); }
  function isCodeLine(ln) {
    if (!ln.chunks || !ln.chunks.length) return false;
    for (var k = 0; k < ln.chunks.length; k++) {
      var st = ln.chunks[k].style || {};
      var mono = st.isMonospace === true || MONO_RE.test(st.name || '');
      if (!mono) return false;
    }
    return true;
  }
  function isBlockquoteLine(t) { return /^[\u2502\u2503\u2551\u2502\uff5c]\s/.test(t) || /^>\s?/.test(t); }
  function isNumberLike(t) { return /^\d{1,4}$/.test(t.trim()) || /^page\s+\d+$/i.test(t.trim()); }
  function endsHyphen(t) { return /-\s*$/.test(t); }
  function hashes(n) { var s = ''; while (n--) s += '#'; return s; }
  function wrapStyle(txt, st) {
    if (st.italic) txt = '*' + txt + '*';
    if (st.bold) txt = '**' + txt + '**';
    return txt;
  }

  // ---- Header / footer strip ----------------------------------------------
  // Lines whose verbatim text repeats in the top/bottom PAGE_FRACTION band of
  // most pages are dropped (running headers/footers, page numbers).
  function stripHeaderFooter(pages) {
    var nPages = pages.length;
    if (nPages < 2) return {};
    var bands = { top: {}, bottom: {} };
    for (var p = 0; p < nPages; p++) {
      var page = pages[p];
      var ph = page.height || 792;
      var topBand = ph * (1 - PAGE_FRACTION), botBand = ph * PAGE_FRACTION;
      var lines = assembleLines(page);
      for (var i = 0; i < lines.length; i++) {
        var ln = lines[i];
        if (isNumberLike(ln.text)) { (bands.bottom[ln.text] || (bands.bottom[ln.text] = {}))[p] = 1; continue; }
        if (ln.y >= topBand) {
          (bands.top[ln.text] || (bands.top[ln.text] = {}))[p] = 1;
        } else if (ln.y <= botBand) {
          (bands.bottom[ln.text] || (bands.bottom[ln.text] = {}))[p] = 1;
        }
      }
    }
    var drop = {};
    function dropBanded(map) {
      for (var t in map) {
        var count = Object.keys(map[t]).length;
        if (count >= Math.ceil(nPages * 0.6) && map[t][0] !== undefined) drop[t] = 1;
      }
    }
    dropBanded(bands.top);
    dropBanded(bands.bottom);
    return drop;
  }

  // ---- Main render ---------------------------------------------------------
  function scribaPdf2Md(data) {
    var pages = data.pages || [];
    var drop = stripHeaderFooter(pages);
    var out = [];
    for (var p = 0; p < pages.length; p++) {
      out.push(renderPage(pages[p], drop));
    }
    return out.join('\n\n');
  }

  function renderPage(page, drop) {
    var lines = assembleLines(page);
    var stats = sizeStats(lines);
    var blocked = function (i) {
      var ln = lines[i];
      return drop[ln.text] !== undefined || isNumberLike(ln.text);
    };
    var tableRuns = detectTables(lines, blocked);

    var out = [];
    var i = 0, n = lines.length;

    var tableIdx = {};
    for (var t = 0; t < tableRuns.length; t++) {
      for (var u = tableRuns[t][0]; u < tableRuns[t][1]; u++) tableIdx[u] = tableRuns[t];
    }

    while (i < n) {
      if (blocked(i)) { i++; continue; }

      var ln = lines[i];
      var text = ln.text;

      // Monospace run -> fenced code block.
      if (isCodeLine(ln)) {
        var k = i;
        while (k < n && !blocked(k) && !isListLine(lines[k].text) && isCodeLine(lines[k])) k++;
        if (k > i) {
          var code = [];
          for (var c = i; c < k; c++) code.push(lines[c].text);
          out.push('```\n' + code.join('\n') + '\n```');
          i = k;
          continue;
        }
      }

      if (tableIdx[i]) {
        var rng = tableIdx[i];
        var tmd = renderTable(lines, rng[0], rng[1]);
        if (tmd) out.push(tmd);
        i = rng[1];
        continue;
      }

      var hlevel = stats.levels[Math.round(ln.height * 10) / 10];
      if (hlevel && !isListLine(text)) {
        out.push(hashes(hlevel) + ' ' + text);
        i++;
        continue;
      }

      if (isListLine(text)) {
        var l = i;
        var ol = OL_RE.test(text);
        var items = [];
        while (l < n && !blocked(l) && isListLine(lines[l].text) && ol === OL_RE.test(lines[l].text)) {
          var it = lines[l].text;
          items.push(it.replace(ol ? OL_RE : BULLET_RE, ''));
          l++;
        }
        out.push(items.map(function (s) { return (ol ? '1. ' : '- ') + s; }).join('\n'));
        i = l;
        continue;
      }

      if (isBlockquoteLine(text)) {
        var q = i;
        var quotes = [];
        while (q < n && !blocked(q) && isBlockquoteLine(lines[q].text)) {
          quotes.push('> ' + lines[q].text.replace(/^>\s?/, ''));
          q++;
        }
        out.push(quotes.join('\n'));
        i = q;
        continue;
      }

      // Plain paragraph (wrapped lines + de-hyphenation). Each line keeps
      // its own emphasis style so a mixed paragraph (italic line, bold line)
      // survives; the join is on the raw text so de-hyphenation sees it.
      var para = []; // {text, style}
      while (i < n && !blocked(i) && !tableIdx[i] && !isCodeLine(lines[i])
             && handleParagraphGap(lines, i, para, out, stats)) {
        var tLine = lines[i].text;
        if (para.length && endsHyphen(para[para.length - 1].text) && /^[a-z0-9]/.test(tLine)) {
          var prev = para.pop();
          para.push({ text: prev.text.replace(/-\s*$/, '') + tLine,
                      style: prev.style || {} });
        } else {
          para.push({ text: tLine, style: lines[i].style || {} });
        }
        i++;
      }
      if (para.length) {
        out.push(para.map(function (p) { return wrapStyle(p.text, p.style); }).join('\n'));
      }
    }
    return out.join('\n\n');
  }

  // Return true if lines[i] can be folded into (or extend) a paragraph;
  // flush `para` onto `out` when a vertical gap ends the current one.
  function handleParagraphGap(lines, i, para, out, stats) {
    var t = lines[i].text;
    if (isListLine(t) || isBlockquoteLine(t)) return false;
    if (stats.levels[Math.round(lines[i].height * 10) / 10]) return false;
    if (i > 0) {
      var prev = lines[i - 1];
      var gap = prev.y - prev.height - lines[i].y;
      if (gap > prev.height * 0.9) {
        if (para.length) {
          out.push(para.map(function (p) { return wrapStyle(p.text, p.style); }).join('\n'));
          para.length = 0;
        }
        // A stack-restart only (blank-line break) still lets this line start
        // a fresh paragraph.
        return true;
      }
    }
    return true;
  }

  function renderTable(lines, from, to) {
    var headerCells = cellsOf(lines[from]);
    var rows = [];
    for (var i = from; i < to; i++) rows.push(cellsOf(lines[i]));
    if (!rows.length) return '';
    var md = [];
    md.push('| ' + headerCells.join(' | ') + ' |');
    md.push('|' + headerCells.map(function () { return '---'; }).join('|') + '|');
    for (var r = 1; r < rows.length; r++) {
      md.push('| ' + rows[r].join(' | ') + ' |');
    }
    return md.join('\n');
  }
  function cellsOf(ln) {
    var cells = [];
    for (var k = 0; k < ln.chunks.length; k++) cells.push(ln.chunks[k].str || '');
    return cells;
  }

  if (typeof window !== 'undefined') window.scribaPdf2Md = scribaPdf2Md;
  // Test hook: feed synthetic getTextContent()-shaped data, read the result.
  if (typeof window !== 'undefined' && window.__scribaTestItems) {
    try {
      window.__scribaTestResult = scribaPdf2Md(window.__scribaTestItems);
    } catch (e) {
      window.__scribaTestResult = { __error: String((e && e.stack) || e) };
    }
  }
})();