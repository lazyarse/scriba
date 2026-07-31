#pragma once

#include "GrammarChecker.h"

// GrammarChecker backed by the vendored harper-core engine exposed through
// the C ABI in vendor/harper-ffi. Spelling is deliberately NOT handled here —
// the app owns spelling via Hunspell (harper's SpellCheck rule is disabled).
//
// Creating an engine loads harper's curated dictionary, so a single shared
// instance should be reused (see Editor).
class HarperEngine : public GrammarChecker
{
public:
    HarperEngine();
    ~HarperEngine() override;

    QList<Issue> check(const QString &text) override;

    // Whether the underlying engine could be initialised.
    bool isAvailable() const { return m_engine != nullptr; }

private:
    void *m_engine = nullptr; // opaque HarperEngine* from harper_init()
};
