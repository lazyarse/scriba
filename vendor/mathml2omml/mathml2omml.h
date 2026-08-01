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
