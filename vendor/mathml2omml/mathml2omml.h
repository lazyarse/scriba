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
#pragma once

#include <expected>
#include <string>
#include <string_view>

struct XmlSink {
    virtual ~XmlSink() = default;
    virtual void startElement(std::string_view qualifiedName) = 0;
    virtual void endElement() = 0;
    virtual void attribute(std::string_view qualifiedName,
                           std::string_view value) = 0;
    virtual void characters(std::string_view text) = 0;
};

struct MathmlToOmml {
    static bool convert(std::string_view mathmlXml, XmlSink &sink);
    static std::expected<std::string, std::string> convert(std::string_view mathmlXml);
};

struct OmmlToMathml {
    static bool convert(std::string_view ommlXml, XmlSink &sink);
    static std::expected<std::string, std::string> convert(std::string_view ommlXml);
};

struct MathmlToLatex {
    // Converts MathML (as produced by OmmlToMathml) to a LaTeX string.
    static std::expected<std::string, std::string> convert(std::string_view mathmlXml);
};
