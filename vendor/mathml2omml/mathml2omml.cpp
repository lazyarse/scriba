#include "mathml2omml.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

// ── Simple XML reader ─────────────────────────────────────────────────────────

class XmlReader {
    const std::string_view s;
    size_t p = 0;

    char peek() const { return p < s.size() ? s[p] : '\0'; }
    char next() { return p < s.size() ? s[p++] : '\0'; }

    void skipWs() {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\t' || s[p] == '\r'))
            ++p;
    }

    std::string readName() {
        size_t start = p;
        if (p < s.size() && (isAlpha(s[p]) || s[p] == '_' || s[p] == ':')) {
            ++p;
            while (p < s.size() && (isAlphaNum(s[p]) || s[p] == '_' || s[p] == '-' || s[p] == ':'))
                ++p;
        }
        return std::string(s.substr(start, p - start));
    }

    std::string readAttrValue() {
        if (p >= s.size()) return {};
        char q = s[p];
        if (q != '"' && q != '\'') return {};
        ++p;
        size_t start = p;
        while (p < s.size() && s[p] != q) ++p;
        std::string raw(s.substr(start, p - start));
        if (p < s.size()) ++p;
        return decodeEnt(raw);
    }

    static void appendUtf8(std::string &out, uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    std::string decodeEnt(std::string_view raw) {
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '&') {
                size_t semi = raw.find(';', i);
                if (semi == std::string_view::npos) { out += raw[i]; continue; }
                std::string_view ent = raw.substr(i + 1, semi - i - 1);
                if (ent == "amp") out += '&';
                else if (ent == "lt") out += '<';
                else if (ent == "gt") out += '>';
                else if (ent == "quot") out += '"';
                else if (ent == "apos") out += '\'';
                else if (ent.size() > 1 && ent[0] == '#') {
                    int base = 10;
                    std::string_view digits = ent.substr(1);
                    if (digits.size() > 1 && digits[0] == 'x') {
                        base = 16;
                        digits.remove_prefix(1);
                    }
                    uint32_t cp = 0;
                    auto [ptr, ec] = std::from_chars(digits.data(),
                                                     digits.data() + digits.size(),
                                                     cp, base);
                    bool valid = ec == std::errc() && ptr == digits.data() + digits.size()
                                 && cp > 0 && cp <= 0x10FFFF
                                 && !(cp >= 0xD800 && cp <= 0xDFFF);
                    if (valid)
                        appendUtf8(out, cp);
                    else
                        out += raw.substr(i, semi - i + 1);
                } else {
                    out += std::string(raw.substr(i, semi - i + 1));
                }
                i = semi;
            } else {
                out += raw[i];
            }
        }
        return out;
    }

    static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    static bool isAlphaNum(char c) { return isAlpha(c) || (c >= '0' && c <= '9'); }

public:
    explicit XmlReader(std::string_view str) : s(str) {}

    struct MmlNode {
        std::string tag;
        std::map<std::string, std::string> attrs;
        std::string text;
        std::vector<MmlNode> children;
    };

    bool seekToFirstElement() {
        while (p < s.size()) {
            skipWs();
            if (p >= s.size()) return false;
            if (s[p] != '<') return false;
            if (p + 1 < s.size() && s[p + 1] == '?') {
                size_t end = s.find("?>", p);
                if (end == std::string_view::npos) return false;
                p = end + 2;
                continue;
            }
            if (p + 3 < s.size() && s[p + 1] == '!' && s[p + 2] == '-' && s[p + 3] == '-') {
                size_t end = s.find("-->", p);
                if (end == std::string_view::npos) return false;
                p = end + 3;
                continue;
            }
            return true;
        }
        return false;
    }

    MmlNode readElement() {
        MmlNode node;
        if (p >= s.size() || s[p] != '<') return node;
        ++p;

        if (p < s.size() && s[p] == '/') {
            ++p;
            readName();
            skipWs();
            if (p < s.size() && s[p] == '>') ++p;
            return node;
        }

        node.tag = readName();
        skipWs();
        while (p < s.size() && s[p] != '>' && s[p] != '/') {
            std::string an = readName();
            skipWs();
            std::string av;
            if (p < s.size() && s[p] == '=') {
                ++p; skipWs();
                av = readAttrValue();
            }
            if (!an.empty()) node.attrs[an] = av;
            skipWs();
        }

        bool selfClose = false;
        if (p < s.size() && s[p] == '/') { selfClose = true; ++p; }
        if (p < s.size() && s[p] == '>') ++p;

        if (selfClose) return node;

        while (p < s.size()) {
            skipWs();
            if (p >= s.size()) break;
            if (s[p] == '<') {
                if (p + 1 < s.size() && s[p + 1] == '/') {
                    p += 2;
                    std::string en = readName();
                    skipWs();
                    if (p < s.size() && s[p] == '>') ++p;
                    if (en == node.tag) break;
                } else {
                    node.children.push_back(readElement());
                }
            } else {
                size_t start = p;
                while (p < s.size() && s[p] != '<') ++p;
                std::string txt(s.substr(start, p - start));
                if (!txt.empty())
                    node.text += decodeEnt(txt);
            }
        }
        return node;
    }
};

// ── Style helpers ─────────────────────────────────────────────────────────────

static std::string defaultStyleForTag(std::string_view tag) {
    return tag == "mi" ? "i" : "p";
}

static std::string mathvariantToStyle(std::string_view variant) {
    if (variant == "normal") return "p";
    if (variant == "italic") return "i";
    if (variant == "bold") return "b";
    if (variant == "bold-italic") return "bi";
    if (variant == "double-struck") return "d";
    if (variant == "fraktur") return "f";
    if (variant == "script") return "s";
    return {};
}

// ── Recursive OMML emitter (sink-based) ──────────────────────────────────────

using MmlNode = XmlReader::MmlNode;

struct StyleContext {
    std::string mathvariant;
};

static void emitOmml(const MmlNode &node, XmlSink &sink,
                      StyleContext ctx = {});

static void emitTextRun(const MmlNode &node, XmlSink &sink,
                        const StyleContext &ctx) {
    std::string style = defaultStyleForTag(node.tag);
    if (auto it = node.attrs.find("mathvariant"); it != node.attrs.end()) {
        if (std::string s = mathvariantToStyle(it->second); !s.empty())
            style = s;
    } else if (!ctx.mathvariant.empty()) {
        if (std::string s = mathvariantToStyle(ctx.mathvariant); !s.empty())
            style = s;
    }

    sink.startElement("m:r");
    if (style != "p") {
        sink.startElement("m:rPr");
        sink.startElement("m:sty");
        sink.attribute("m:val", style);
        sink.endElement(); // m:sty
        sink.endElement(); // m:rPr
    }
    sink.startElement("m:t");
    sink.attribute("xml:space", "preserve");
    sink.characters(node.text);
    sink.endElement(); // m:t
    sink.endElement(); // m:r
}

static void emitOmml(const MmlNode &node, XmlSink &sink,
                      StyleContext ctx) {
    const std::string &t = node.tag;

    if (t == "math" || t == "semantics") {
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        return;
    }
    if (t == "annotation" || t == "annotation-xml") return;

    if (t == "mi" || t == "mn" || t == "mo" || t == "mtext" || t == "ms")
        return emitTextRun(node, sink, ctx);
    if (t == "mspace") {
        // Drop ordinary spaces, but emit m:br for line breaks
        if (auto it = node.attrs.find("linebreak");
            it != node.attrs.end() && it->second == "newline") {
            sink.startElement("m:br");
            sink.endElement();
        }
        return;
    }
    if (t == "mphantom") {
        sink.startElement("m:phant");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        return;
    }
    if (t == "mmultiscripts") {
        auto mpIt = std::ranges::find_if(node.children,
            [](const MmlNode &c) { return c.tag == "mprescripts"; });
        bool hasPre = mpIt != node.children.end();
        size_t mp = static_cast<size_t>(std::ranges::distance(node.children.begin(), mpIt));

        if (hasPre) {
            sink.startElement("m:sPre");
            sink.startElement("m:e");
            if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
            sink.endElement();
            sink.startElement("m:sub");
            if (mp + 1 < node.children.size()) emitOmml(node.children[mp + 1], sink, ctx);
            sink.endElement();
            sink.startElement("m:sup");
            if (mp + 2 < node.children.size()) emitOmml(node.children[mp + 2], sink, ctx);
            sink.endElement();
            sink.endElement();
        } else {
            for (const auto &c : node.children) emitOmml(c, sink, ctx);
        }
        return;
    }

    if (t == "mrow") {
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        return;
    }

    if (t == "msup") {
        sink.startElement("m:sSup");
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:sup");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "msub") {
        sink.startElement("m:sSub");
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:sub");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "msubsup") {
        sink.startElement("m:sSubSup");
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:sub");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.startElement("m:sup");
        if (node.children.size() >= 3) emitOmml(node.children[2], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "mfrac") {
        sink.startElement("m:f");
        sink.startElement("m:num");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:den");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "msqrt") {
        sink.startElement("m:rad");
        sink.startElement("m:deg");
        sink.endElement();
        sink.startElement("m:e");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "mroot") {
        sink.startElement("m:rad");
        sink.startElement("m:deg");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "mover") {
        sink.startElement("m:limUpp");
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:lim");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "munder") {
        sink.startElement("m:limLow");
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:lim");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "munderover") {
        sink.startElement("m:limLow");
        sink.startElement("m:e");
        sink.startElement("m:limUpp");
        sink.startElement("m:e");
        if (node.children.size() >= 1) emitOmml(node.children[0], sink, ctx);
        sink.endElement();
        sink.startElement("m:lim");
        if (node.children.size() >= 3) emitOmml(node.children[2], sink, ctx);
        sink.endElement();
        sink.endElement();
        sink.endElement();
        sink.startElement("m:lim");
        if (node.children.size() >= 2) emitOmml(node.children[1], sink, ctx);
        sink.endElement();
        sink.endElement();
        return;
    }

    if (t == "mtable") {
        sink.startElement("m:m");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        return;
    }

    if (t == "mtr") {
        sink.startElement("m:mr");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        return;
    }

    if (t == "mtd") {
        sink.startElement("m:mc");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        return;
    }

    if (t == "mstyle") {
        StyleContext childCtx = ctx;
        if (auto mvIt = node.attrs.find("mathvariant"); mvIt != node.attrs.end())
            childCtx.mathvariant = mvIt->second;
        for (const auto &c : node.children) emitOmml(c, sink, childCtx);
        return;
    }

    if (t == "mpadded" || t == "merror") {
        sink.startElement("m:box");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        return;
    }

    if (t == "menclose") {
        sink.startElement("m:borderBox");
        for (const auto &c : node.children) emitOmml(c, sink, ctx);
        sink.endElement();
        return;
    }

    // Unknown — process children
    for (const auto &c : node.children) emitOmml(c, sink, ctx);
}

// ── OMML → MathML helpers ──────────────────────────────────────────────────

static std::string stripNs(std::string_view tag) {
    size_t colon = tag.find(':');
    return colon != std::string_view::npos ? std::string(tag.substr(colon + 1))
                                           : std::string(tag);
}

static std::optional<std::size_t> findChild(const MmlNode &node,
                                            std::string_view localTag) {
    auto it = std::ranges::find_if(node.children, [localTag](const MmlNode &c) {
        return stripNs(c.tag) == localTag;
    });
    if (it == node.children.end()) return std::nullopt;
    return static_cast<std::size_t>(std::ranges::distance(node.children.begin(), it));
}

static std::optional<std::string> getAttr(const MmlNode &node,
                                          std::string_view localKey) {
    for (const auto &[k, v] : node.attrs) {
        if (stripNs(k) == localKey) return v;
    }
    return std::nullopt;
}

static std::string styleToMathvariant(std::string_view scr, std::string_view sty) {
    std::string s = scr.empty() ? "roman" : std::string(scr);
    std::string t = sty.empty() ? "p" : std::string(sty);

    if (s == "roman") {
        if (t == "p") return "normal";
        if (t == "b") return "bold";
        if (t == "i") return "italic";
        if (t == "bi") return "bold-italic";
    }
    if (s == "script") {
        if (t == "p" || t == "i") return "script";
        if (t == "b" || t == "bi") return "bold-script";
    }
    if (s == "fraktur") {
        if (t == "p" || t == "i") return "fraktur";
        if (t == "b" || t == "bi") return "bold-fraktur";
    }
    if (s == "double-struck") {
        if (t == "p" || t == "i") return "double-struck";
        if (t == "b" || t == "bi") return "bold-double-struck";
    }
    if (s == "sans-serif") {
        if (t == "p") return "sans-serif";
        if (t == "b") return "bold-sans-serif";
        if (t == "i") return "sans-serif-italic";
        if (t == "bi") return "sans-serif-bold-italic";
    }
    if (s == "monospace") return "monospace";

    return "normal";
}

static std::string guessTokenType(std::string_view text) {
    if (text.empty()) return "mi";
    bool allDigits = true;
    bool hasOperator = false;
    for (unsigned char c : text) {
        if (c >= '0' && c <= '9') continue;
        if (c == '.' && allDigits) continue;
        allDigits = false;
        if (c == '+' || c == '-' || c == '=' || c == '<' || c == '>' ||
            c == 0xD7 || c == 0xF7 || c == 0xB1 || c == 0xAF || c == 0x7E ||
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' ||
            c == '}' || c == '|' || c == '/' || c == '\\' || c == '*' ||
            c == ',' || c == ';' || c == ':' || c == '!' || c == '?' ||
            c == 0xB7 || c == 0x22 || c == 0xAC || c == 0xA8)
            hasOperator = true;
        // Multi-byte UTF-8 operators
        if (c >= 0xC0) {
            // Common Unicode math operators in the U+2200-U+22FF range
            hasOperator = true;
        }
    }
    if (allDigits) return "mn";
    if (hasOperator) return "mo";
    return "mi";
}

// Read property value from a Pr element's child element's m:val attribute.
// e.g. <m:fPr><m:type m:val="noBar"/></m:fPr> => readProp(fPr, "type") => "noBar"
static std::optional<std::string> readProp(const MmlNode &prNode,
                                           std::string_view propName) {
    auto idx = findChild(prNode, propName);
    if (!idx) return std::nullopt;
    return getAttr(prNode.children[*idx], "val");
}

// Check if a property child element exists (presence check) or has val="1"/"true"
static bool propFlag(const MmlNode &node, std::string_view prTag,
                     std::string_view propName)
{
    auto prIdx = findChild(node, prTag);
    if (!prIdx) return false;
    auto propIdx = findChild(node.children[*prIdx], propName);
    if (!propIdx) return false;
    // If the property element has no val attribute, its presence is enough
    auto v = getAttr(node.children[*prIdx].children[*propIdx], "val");
    return !v || v->empty() || *v == "1" || *v == "true";
}

// Positions of e/sub/sup children (single pass, last occurrence wins).
struct ScriptParts {
    std::optional<std::size_t> e, sub, sup;
};

static ScriptParts findScriptParts(const MmlNode &node) {
    ScriptParts parts;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        std::string ct = stripNs(node.children[i].tag);
        if (ct == "e") parts.e = i;
        else if (ct == "sub") parts.sub = i;
        else if (ct == "sup") parts.sup = i;
    }
    return parts;
}

// ── Recursive MathML emitter (reverse of emitOmml) ─────────────────────────

static void emitMathml(const MmlNode &node, XmlSink &sink);

static void emitContainerChildren(const MmlNode &node, XmlSink &sink) {
    for (const auto &c : node.children) emitMathml(c, sink);
}

static void emitMathml(const MmlNode &node, XmlSink &sink) {
    std::string tag = stripNs(node.tag);
    if (tag.empty()) return;

    // ── Root elements ──────────────────────────────────────────────────────
    if (tag == "oMath" || tag == "oMathPara") {
        bool displayBlock = (tag == "oMathPara");
        auto oMathIdx = findChild(node, "oMath");
        sink.startElement("math");
        if (displayBlock) sink.attribute("display", "block");
        if (oMathIdx) emitContainerChildren(node.children[*oMathIdx], sink);
        else emitContainerChildren(node, sink);
        sink.endElement();
        return;
    }

    // ── Property elements — silently skip (consumed by parent processing) ──
    if (tag == "rPr" || tag == "tPr" || tag == "fPr" || tag == "naryPr" ||
        tag == "dPr" || tag == "accPr" || tag == "barPr" || tag == "groupChrPr" ||
        tag == "radPr" || tag == "phantPr" || tag == "sSupPr" || tag == "sSubPr" ||
        tag == "sSubSupPr" || tag == "sPrePr" || tag == "eqArrPr" ||
        tag == "borderBoxPr" || tag == "boxPr" || tag == "ctrlPr" ||
        tag == "spPr" || tag == "argPr" || tag == "mPr" || tag == "mcPr" ||
        tag == "mrPr" || tag == "oMathParaPr" || tag == "mathPr")
        return;

    // ── Container / argument elements — transparent passthrough ────────────
    if (tag == "e" || tag == "num" || tag == "den" || tag == "sup" ||
        tag == "sub" || tag == "lim" || tag == "deg" || tag == "fName") {
        emitContainerChildren(node, sink);
        return;
    }

    // ── Token element: m:r (run) ──────────────────────────────────────────
    if (tag == "r") {
        auto rPrIdx = findChild(node, "rPr");
        auto tIdx = findChild(node, "t");

        bool nor = false;
        std::string scr, sty;
        if (rPrIdx) {
            const MmlNode &pr = node.children[*rPrIdx];
            nor = findChild(pr, "nor").has_value();
            if (auto scrIdx = findChild(pr, "scr"))
                scr = getAttr(pr.children[*scrIdx], "val").value_or("");
            if (auto styIdx = findChild(pr, "sty"))
                sty = getAttr(pr.children[*styIdx], "val").value_or("");
            if (sty.empty() && findChild(pr, "b")) sty = "b";
            if (sty.empty() && findChild(pr, "i")) sty = "i";
        }

        std::string text;
        if (tIdx) text = node.children[*tIdx].text;

        std::string mathvariant = styleToMathvariant(scr, sty);
        std::string tokType = nor ? "mtext" : guessTokenType(text);

        sink.startElement(tokType);
        if (mathvariant != "normal") {
            sink.attribute("mathvariant", mathvariant);
        }
        if (!text.empty()) {
            sink.characters(text);
        }
        sink.endElement();
        return;
    }

    // ── m:sSup → msup ─────────────────────────────────────────────────────
    if (tag == "sSup") {
        ScriptParts p = findScriptParts(node);
        sink.startElement("msup");
        if (p.e) emitContainerChildren(node.children[*p.e], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (p.sup) emitContainerChildren(node.children[*p.sup], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:sSub → msub ─────────────────────────────────────────────────────
    if (tag == "sSub") {
        ScriptParts p = findScriptParts(node);
        sink.startElement("msub");
        if (p.e) emitContainerChildren(node.children[*p.e], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (p.sub) emitContainerChildren(node.children[*p.sub], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:sSubSup → msubsup ───────────────────────────────────────────────
    if (tag == "sSubSup") {
        ScriptParts p = findScriptParts(node);
        sink.startElement("msubsup");
        if (p.e) emitContainerChildren(node.children[*p.e], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (p.sub) emitContainerChildren(node.children[*p.sub], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (p.sup) emitContainerChildren(node.children[*p.sup], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:sPre → mmultiscripts + mprescripts ──────────────────────────────
    if (tag == "sPre") {
        ScriptParts p = findScriptParts(node);
        sink.startElement("mmultiscripts");
        if (p.e) emitContainerChildren(node.children[*p.e], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.startElement("mprescripts");
        sink.endElement();
        if (p.sub) emitContainerChildren(node.children[*p.sub], sink);
        else { sink.startElement("none"); sink.endElement(); }
        if (p.sup) emitContainerChildren(node.children[*p.sup], sink);
        else { sink.startElement("none"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:f → mfrac (with optional bevelled/linethickness) ────────────────
    if (tag == "f") {
        auto fPrIdx = findChild(node, "fPr");
        auto numIdx = findChild(node, "num");
        auto denIdx = findChild(node, "den");

        std::string type;
        if (fPrIdx) type = readProp(node.children[*fPrIdx], "type").value_or("");

        sink.startElement("mfrac");
        if (type == "lin" || type == "noBar") {
            sink.attribute("linethickness", "0");
        } else if (type == "skw") {
            sink.attribute("bevelled", "true");
        }
        if (numIdx) emitContainerChildren(node.children[*numIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (denIdx) emitContainerChildren(node.children[*denIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:rad → msqrt (empty deg) or mroot (deg with content) ─────────────
    if (tag == "rad") {
        auto degIdx = findChild(node, "deg");
        auto eIdx = findChild(node, "e");

        bool degHide = propFlag(node, "radPr", "degHide");
        bool hasDeg = degIdx && (!node.children[*degIdx].children.empty()
                                 || !node.children[*degIdx].text.empty());

        if (!hasDeg || degHide) {
            sink.startElement("msqrt");
            if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
            sink.endElement();
        } else {
            sink.startElement("mroot");
            if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
            else { sink.startElement("mrow"); sink.endElement(); }
            emitContainerChildren(node.children[*degIdx], sink);
            sink.endElement();
        }
        return;
    }

    // ── m:limUpp → mover ──────────────────────────────────────────────────
    if (tag == "limUpp") {
        auto eIdx = findChild(node, "e");
        auto limIdx = findChild(node, "lim");
        sink.startElement("mover");
        if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (limIdx) emitContainerChildren(node.children[*limIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:limLow → munder ─────────────────────────────────────────────────
    if (tag == "limLow") {
        auto eIdx = findChild(node, "e");
        auto limIdx = findChild(node, "lim");
        sink.startElement("munder");
        if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        if (limIdx) emitContainerChildren(node.children[*limIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.endElement();
        return;
    }

    // ── m:nary → n-ary operator (sum, prod, int) ──────────────────────────
    if (tag == "nary") {
        auto naryPrIdx = findChild(node, "naryPr");
        auto subIdx = findChild(node, "sub");
        auto supIdx = findChild(node, "sup");
        auto eIdx = findChild(node, "e");

        std::string chr, limLoc;
        if (naryPrIdx) {
            chr = readProp(node.children[*naryPrIdx], "chr").value_or("");
            limLoc = readProp(node.children[*naryPrIdx], "limLoc").value_or("");
        }

        bool useUnderOver = (limLoc == "undOvr");
        bool hasSub = subIdx && (!node.children[*subIdx].children.empty()
                                 || !node.children[*subIdx].text.empty());
        bool hasSup = supIdx && (!node.children[*supIdx].children.empty()
                                 || !node.children[*supIdx].text.empty());

        sink.startElement("mrow");

        if (useUnderOver && hasSub && hasSup) {
            sink.startElement("munderover");
            sink.startElement("mo");
            if (!chr.empty()) sink.characters(chr);
            sink.endElement();
            emitContainerChildren(node.children[*subIdx], sink);
            emitContainerChildren(node.children[*supIdx], sink);
            sink.endElement();
        } else if (hasSub && hasSup) {
            sink.startElement("msubsup");
            sink.startElement("mo");
            if (!chr.empty()) sink.characters(chr);
            sink.endElement();
            emitContainerChildren(node.children[*subIdx], sink);
            emitContainerChildren(node.children[*supIdx], sink);
            sink.endElement();
        } else if (hasSub) {
            sink.startElement("msub");
            sink.startElement("mo");
            if (!chr.empty()) sink.characters(chr);
            sink.endElement();
            emitContainerChildren(node.children[*subIdx], sink);
            sink.endElement();
        } else if (hasSup) {
            sink.startElement("msup");
            sink.startElement("mo");
            if (!chr.empty()) sink.characters(chr);
            sink.endElement();
            emitContainerChildren(node.children[*supIdx], sink);
            sink.endElement();
        } else {
            sink.startElement("mo");
            if (!chr.empty()) sink.characters(chr);
            sink.endElement();
        }

        if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
        sink.endElement(); // mrow
        return;
    }

    // ── m:d → delimiters (parentheses, brackets) ──────────────────────────
    if (tag == "d") {
        auto dPrIdx = findChild(node, "dPr");
        auto eIdx = findChild(node, "e");

        std::string begChr = "(", endChr = ")", sepChr;
        if (dPrIdx) {
            std::string b = readProp(node.children[*dPrIdx], "begChr").value_or("");
            if (!b.empty()) begChr = b;
            std::string en = readProp(node.children[*dPrIdx], "endChr").value_or("");
            if (!en.empty()) endChr = en;
            sepChr = readProp(node.children[*dPrIdx], "sepChr").value_or("");
        }

        sink.startElement("mrow");
        sink.startElement("mo");
        if (!begChr.empty()) {
            if (begChr == "|") sink.characters("|");
            else if (begChr == "||") sink.characters("\u2016");
            else sink.characters(begChr);
        }
        sink.endElement();

        if (eIdx) emitContainerChildren(node.children[*eIdx], sink);

        sink.startElement("mo");
        if (!endChr.empty()) {
            if (endChr == "|") sink.characters("|");
            else if (endChr == "||") sink.characters("\u2016");
            else sink.characters(endChr);
        }
        sink.endElement();
        sink.endElement(); // mrow
        return;
    }

    // ── m:acc → accent (hat, tilde, dot, etc.) ────────────────────────────
    if (tag == "acc") {
        auto accPrIdx = findChild(node, "accPr");
        auto eIdx = findChild(node, "e");

        std::string chr = "\u0302";
        if (accPrIdx) {
            std::string c = readProp(node.children[*accPrIdx], "chr").value_or("");
            if (!c.empty()) chr = c;
        }

        sink.startElement("mover");
        if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
        else { sink.startElement("mrow"); sink.endElement(); }
        sink.startElement("mo");
        sink.characters(chr);
        sink.endElement();
        sink.endElement();
        return;
    }

    // ── m:bar → overline/underline ────────────────────────────────────────
    if (tag == "bar") {
        auto barPrIdx = findChild(node, "barPr");
        auto eIdx = findChild(node, "e");

        std::string pos = "top";
        if (barPrIdx) {
            std::string p = readProp(node.children[*barPrIdx], "pos").value_or("");
            if (!p.empty()) pos = p;
        }

        if (pos == "top" || pos == "topBot") {
            sink.startElement("mover");
            if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
            else { sink.startElement("mrow"); sink.endElement(); }
            sink.startElement("mo");
            sink.characters("\u00AF");
            sink.endElement();
            sink.endElement();
        }
        if (pos == "bot" || pos == "topBot") {
            sink.startElement("munder");
            if (eIdx && pos != "top") emitContainerChildren(node.children[*eIdx], sink);
            else if (eIdx) {} // already emitted in mover
            else { sink.startElement("mrow"); sink.endElement(); }
            sink.startElement("mo");
            sink.characters("\u00AF");
            sink.endElement();
            sink.endElement();
        }
        return;
    }

    // ── m:func → function application (e.g., sin x) ──────────────────────
    if (tag == "func") {
        auto fNameIdx = findChild(node, "fName");
        auto eIdx = findChild(node, "e");
        sink.startElement("mrow");
        if (fNameIdx) emitContainerChildren(node.children[*fNameIdx], sink);
        sink.startElement("mo");
        sink.characters("\u2061");
        sink.endElement();
        if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
        sink.endElement();
        return;
    }

    // ── m:groupChr → group character (under/over brace) ───────────────────
    if (tag == "groupChr") {
        auto grPrIdx = findChild(node, "groupChrPr");
        auto eIdx = findChild(node, "e");

        std::string chr = "\u23DF";
        std::string pos = "bot";
        if (grPrIdx) {
            std::string c = readProp(node.children[*grPrIdx], "chr").value_or("");
            if (!c.empty()) chr = c;
            std::string p = readProp(node.children[*grPrIdx], "pos").value_or("");
            if (!p.empty()) pos = p;
        }

        if (pos == "top") {
            sink.startElement("mover");
            if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
            else { sink.startElement("mrow"); sink.endElement(); }
            sink.startElement("mo");
            sink.characters(chr);
            sink.endElement();
            sink.endElement();
        } else {
            sink.startElement("munder");
            if (eIdx) emitContainerChildren(node.children[*eIdx], sink);
            else { sink.startElement("mrow"); sink.endElement(); }
            sink.startElement("mo");
            sink.characters(chr);
            sink.endElement();
            sink.endElement();
        }
        return;
    }

    // ── m:m → mtable ──────────────────────────────────────────────────────
    if (tag == "m") {
        sink.startElement("mtable");
        emitContainerChildren(node, sink);
        sink.endElement();
        return;
    }

    // ── m:mr → mtr ────────────────────────────────────────────────────────
    if (tag == "mr") {
        sink.startElement("mtr");
        emitContainerChildren(node, sink);
        sink.endElement();
        return;
    }

    // ── m:mc → mtd ────────────────────────────────────────────────────────
    if (tag == "mc") {
        sink.startElement("mtd");
        emitContainerChildren(node, sink);
        sink.endElement();
        return;
    }

    // ── m:eqArr → mtable (equation array, similar to matrix) ─────────────
    if (tag == "eqArr") {
        sink.startElement("mtable");
        emitContainerChildren(node, sink);
        sink.endElement();
        return;
    }

    // ── m:phant → mphantom (or mrow/mpadded based on properties) ─────────
    if (tag == "phant") {
        auto phantPrIdx = findChild(node, "phantPr");
        bool show = false, zeroWid = false;
        if (phantPrIdx) {
            const MmlNode &pr = node.children[*phantPrIdx];
            show = findChild(pr, "show").has_value();
            // Even without the element, if zeroWid child exists, it's true
            zeroWid = findChild(pr, "zeroWid").has_value();
        }

        if (show) {
            if (zeroWid) {
                sink.startElement("mpadded");
                sink.attribute("width", "0");
                emitContainerChildren(node, sink);
                sink.endElement();
            } else {
                emitContainerChildren(node, sink);
            }
        } else {
            sink.startElement("mphantom");
            emitContainerChildren(node, sink);
            sink.endElement();
        }
        return;
    }

    // ── m:borderBox → menclose ────────────────────────────────────────────
    if (tag == "borderBox") {
        sink.startElement("menclose");
        sink.attribute("notation", "box");
        emitContainerChildren(node, sink);
        sink.endElement();
        return;
    }

    // ── m:box → transparent (process children) ────────────────────────────
    if (tag == "box") {
        emitContainerChildren(node, sink);
        return;
    }

    // ── m:br → line break ─────────────────────────────────────────────────
    if (tag == "br") {
        sink.startElement("mspace");
        sink.attribute("linebreak", "newline");
        sink.endElement();
        return;
    }

    // ── Unknown — process children ─────────────────────────────────────────
    emitContainerChildren(node, sink);
}

// ── String-building sink (keeps string overload working) ──────────────────────

class StringSink : public XmlSink {
    std::string out_;
    bool pendingAttr_ = false;
    std::vector<std::string> stack_;

    static std::string escXml(std::string_view s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': r += "&amp;"; break;
                case '<': r += "&lt;"; break;
                case '>': r += "&gt;"; break;
                default: r += c;
            }
        }
        return r;
    }

public:
    void startElement(std::string_view name) override {
        if (pendingAttr_) {
            out_ += '>';
            pendingAttr_ = false;
        }
        out_ += '<';
        out_ += name;
        pendingAttr_ = true;
        stack_.emplace_back(name);
    }

    void endElement() override {
        if (pendingAttr_) {
            out_ += '/';
            out_ += '>';
            pendingAttr_ = false;
            stack_.pop_back();
            return;
        }
        out_ += "</";
        out_ += stack_.back();
        out_ += '>';
        stack_.pop_back();
    }

    void attribute(std::string_view name, std::string_view value) override {
        out_ += ' ';
        out_ += name;
        out_ += "=\"";
        out_ += escXml(value);
        out_ += '"';
    }

    void characters(std::string_view text) override {
        if (pendingAttr_) {
            out_ += '>';
            pendingAttr_ = false;
        }
        out_ += escXml(text);
    }

    std::string result() const { return out_; }
};

// ── Public API ────────────────────────────────────────────────────────────────

bool MathmlToOmml::convert(std::string_view mathmlXml, XmlSink &sink)
{
    if (mathmlXml.empty()) return false;

    XmlReader reader(mathmlXml);
    if (!reader.seekToFirstElement()) return false;

    MmlNode root = reader.readElement();

    sink.startElement("m:oMath");
    sink.attribute("xmlns:m",
        "http://schemas.openxmlformats.org/officeDocument/2006/math");
    emitOmml(root, sink);
    sink.endElement();
    return true;
}

std::expected<std::string, std::string> MathmlToOmml::convert(std::string_view mathmlXml)
{
    if (mathmlXml.empty())
        return std::unexpected("empty input");

    XmlReader reader(mathmlXml);
    if (!reader.seekToFirstElement())
        return std::unexpected("no root element found");

    MmlNode root = reader.readElement();

    StringSink sink;
    sink.startElement("m:oMath");
    sink.attribute("xmlns:m",
        "http://schemas.openxmlformats.org/officeDocument/2006/math");
    emitOmml(root, sink);
    sink.endElement();
    return sink.result();
}

// ── OMML → MathML public API ──────────────────────────────────────────────

bool OmmlToMathml::convert(std::string_view ommlXml, XmlSink &sink)
{
    if (ommlXml.empty()) return false;

    XmlReader reader(ommlXml);
    if (!reader.seekToFirstElement()) return false;

    MmlNode root = reader.readElement();

    emitMathml(root, sink);
    return true;
}

std::expected<std::string, std::string> OmmlToMathml::convert(std::string_view ommlXml)
{
    if (ommlXml.empty())
        return std::unexpected("empty input");

    XmlReader reader(ommlXml);
    if (!reader.seekToFirstElement())
        return std::unexpected("no root element found");

    MmlNode root = reader.readElement();

    StringSink sink;
    emitMathml(root, sink);
    return sink.result();
}
