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

// Reverse-parse the source Scriba's chart helpers emit back into the data the
// helper dialogs consume, so an existing rendered chart can be re-opened in
// its dialog with the fields pre-filled. Pure logic — no Qt widgets — so it is
// unit-testable without WebEngine.
//
// The parsers target the exact output formats of the Chart Builder
// (ChartDialog), Stock Chart (StockChartDialog) and Mermaid Chart
// (MermaidDialog) dialogs. Diagrams that were not produced by those dialogs
// (or are too free-form) fail to parse; callers then fall back to editing the
// raw source.

#include "EChartsParser.h"
#include "MermaidParser.h"
