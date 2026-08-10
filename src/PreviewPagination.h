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

#include "PrintOptions.h"
#include <QString>

namespace PreviewPagination {

// Print-layout chrome for page-break mode: a grey canvas with a centred white
// page sized to the print page's content box, "Page N" separators and
// code-split markers. Injected into the preview's #center-css style element.
QString layoutCss(int contentWidthPx, int contentHeightPx,
                  int marginTopPx, int marginSidePx, int marginBottomPx);

// The paginator script. Defines window.scribaPaginate (walks the top-level
// blocks of #scriba-content and inserts .scriba-pb separators / .scriba-split-
// marker breaks honouring the print options) and window.scribaFitZoom (fit the
// page width to the pane), plus a resize listener. Returns the whole <script>.
QString paginatorScript(const PrintOptions::Options &opts, int contentHeightPx);

// Wire the paginator into the preview's update chains: both the initial
// DOMContentLoaded render pass and every scribaUpdate heavy-render pass re-run
// the paginator when window.scribaPaginate exists.
QString patchIncrementalPaginate(const QString &fullHtml);

}
