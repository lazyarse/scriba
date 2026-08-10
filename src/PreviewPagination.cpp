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
#include "PreviewPagination.h"

namespace PreviewPagination {

QString layoutCss(int contentWidthPx, int contentHeightPx,
                  int marginTopPx, int marginSidePx, int marginBottomPx)
{
    // `#scriba-content` is the white page: content-box sized to the print
    // page's content area, padded by the page margins. The separators and
    // split markers live inside it (position:relative anchor for the markers).
    return QStringLiteral(
        "body#preview{background:#d9d9d9!important;}"
        "#scriba-content{width:%1px;margin:0 auto;background:#fff;"
        "box-shadow:0 1px 10px rgba(0,0,0,.28);padding:%2px %3px %4px;"
        "min-height:%5px;position:relative;}"
        "pre{position:relative;}"
        ".scriba-pb{height:0;border-top:2px dashed #bbb;margin:0 0 28px;position:relative;}"
        ".scriba-pb span{position:absolute;left:50%;top:-11px;transform:translateX(-50%);"
        "background:#fff;padding:0 12px;font:11px/20px system-ui,sans-serif;color:#888;}"
        ".scriba-split-marker{position:absolute;left:0;right:0;height:0;"
        "border-top:2px dashed #d77;pointer-events:none;}"
        ".scriba-split-marker::after{content:'continues';position:absolute;right:6px;top:-12px;"
        "font:10px/16px system-ui,sans-serif;color:#c66;}"
    ).arg(contentWidthPx).arg(marginTopPx).arg(marginSidePx)
     .arg(marginBottomPx).arg(contentHeightPx);
}

QString paginatorScript(const PrintOptions::Options &opts, int contentHeightPx)
{
    const bool split = opts.codeSplit != PrintOptions::CodeSplit::NeverSplit;
    return QStringLiteral(
        "<script>(function(){"
        "var contentH=%1;"
        "var opts={split:%2,keepTables:%3,keepHeadings:%4,keepFigures:%5,orphanControl:%6};"
        "function fitZoom(){"
        "  var c=document.getElementById('scriba-content');"
        "  if(!c||!contentH)return;"
        "  var cw=c.clientWidth;"
        "  if(cw<=0)return;"
        "  var z=Math.min(1,window.innerWidth/cw);"
        "  document.body.style.zoom=z.toFixed(4);"
        "}"
        "window.scribaFitZoom=fitZoom;"
        "function isH(n){return /^H[1-6]$/.test(n.tagName);}"
        "function hasClass(n,s){return n.className&&String(n.className).indexOf(s)>-1;}"
        "function outerH(n){"
        "  var r=n.getBoundingClientRect();"
        "  var cs=getComputedStyle(n);"
        "  return r.height+((parseFloat(cs.marginTop)||0)+(parseFloat(cs.marginBottom)||0));"
        "}"
        "function lineH(n){"
        "  var cs=getComputedStyle(n);"
        "  var lh=parseFloat(cs.lineHeight);"
        "  return (lh&&cs.lineHeight!=='normal')?lh:((parseFloat(cs.fontSize)||16)*1.2);"
        "}"
        "function isFigure(n){"
        "  var c=String(n.className||'');"
        "  return n.tagName==='BLOCKQUOTE'||n.tagName==='IMG'||n.tagName==='TABLE'"
        "    ||c.indexOf('admonition')>-1||c.indexOf('mermaid')>-1"
        "    ||c.indexOf('katex-display')>-1||c.indexOf('echarts')>-1"
        "    ||c.indexOf('scriba-chart-wrap')>-1||c.indexOf('scriba-mathbox')>-1;"
        "}"
        "function flow(H,used,wantMarkers){"
        "  var marks=[];"
        "  if(H<=0)return{used:used,marks:marks};"
        "  var rem=contentH-used;"
        "  if(H<=rem)return{used:used+H,marks:marks};"
        "  var rest=H-rem;"
        "  if(wantMarkers)marks.push(rem);"
        "  var pages=Math.floor(rest/contentH);"
        "  var tail=rest-pages*contentH;"
        "  if(wantMarkers){for(var k=1;k<=pages;k++)marks.push(rem+k*contentH);}"
        "  return{used:tail,marks:marks};"
        "}"
        "window.scribaPaginate=function(){"
        "  var content=document.getElementById('scriba-content');"
        "  if(!content)return -1;"
        "  fitZoom();"
        "  var arts=content.querySelectorAll('.scriba-pb,.scriba-split-marker');"
        "  for(var i=0;i<arts.length;i++)arts[i].remove();"
        "  var blocks=[];"
        "  for(var j=0;j<content.children.length;j++)blocks.push(content.children[j]);"
        "  var used=0,pageNo=1,total=0;"
        "  function insertSep(before){"
        "    var sep=document.createElement('div');"
        "    sep.className='scriba-pb';"
        "    var lbl=document.createElement('span');"
        "    lbl.textContent='Page '+pageNo;"
        "    sep.appendChild(lbl);"
        "    content.insertBefore(sep,before);"
        "    pageNo++;total++;used=0;"
        "  }"
        "  function insertMarker(pre,off){"
        "    var m=document.createElement('div');"
        "    m.className='scriba-split-marker';"
        "    m.style.top=Math.round(off)+'px';"
        "    pre.appendChild(m);"
        "  }"
        "  for(var bi=0;bi<blocks.length;bi++){"
        "    var b=blocks[bi];"
        "    var H=outerH(b);"
        "    if(hasClass(b,'scriba-page-break')){"
        "      if(used>0)insertSep(b);"
        "      used=flow(H,0,false).used;"
        "      continue;"
        "    }"
        "    var keep=hasClass(b,'scriba-keep');"
        "    if(!keep&&opts.keepTables&&b.tagName==='TABLE')keep=true;"
        "    if(!keep&&opts.keepFigures&&isFigure(b))keep=true;"
        "    var H2=H,next=null;"
        "    if(opts.keepHeadings&&isH(b)&&bi+1<blocks.length){"
        "      var nxt=blocks[bi+1];"
        "      if(!isH(nxt)&&!hasClass(nxt,'scriba-page-break')"
        "         &&nxt.tagName!=='TABLE'&&nxt.tagName!=='PRE'&&!isFigure(nxt)"
        "         &&nxt.tagName!=='UL'&&nxt.tagName!=='OL'&&nxt.tagName!=='SECTION'){"
        "        next=nxt;H2=H+outerH(nxt);"
        "      }"
        "    }"
        "    if(keep&&H2<=contentH&&used+H2>contentH){insertSep(b);used=0;}"
        "    var res;"
        "    if(opts.split&&b.tagName==='PRE'&&H>contentH){"
        "      res=flow(H,used,true);"
        "      for(var mi=0;mi<res.marks.length;mi++)insertMarker(b,res.marks[mi]);"
        "      used=res.used;"
        "    }else if(keep){"
        "      res=flow(H,used,false);used=res.used;"
        "    }else{"
        "      if(b.tagName==='P'&&opts.orphanControl){"
        "        var lh=lineH(b),rem=contentH-used;"
        "        if(used>0&&H>rem&&H<=contentH){"
        "          var tail=H-rem;"
        "          if(tail<2*lh){insertSep(b);used=0;}"
        "        }"
        "      }"
        "      res=flow(H,used,false);used=res.used;"
        "    }"
        "  }"
        "  return total;"
        "};"
        "window.addEventListener('resize',function(){fitZoom();});"
        "fitZoom();"
        "})();</script>"
    ).arg(contentHeightPx)
     .arg(split ? QStringLiteral("true") : QStringLiteral("false"))
     .arg(opts.keepTables ? QStringLiteral("true") : QStringLiteral("false"))
     .arg(opts.keepHeadings ? QStringLiteral("true") : QStringLiteral("false"))
     .arg(opts.keepFigures ? QStringLiteral("true") : QStringLiteral("false"))
     .arg(opts.orphanControl ? QStringLiteral("true") : QStringLiteral("false"));
}

QString patchIncrementalPaginate(const QString &fullHtml)
{
    QString html = fullHtml;

    // Every scribaUpdate heavy-render chain ends with restoreScroll(); re-run
    // the paginator right after so typed edits re-paginate immediately.
    const QString chainHook = QStringLiteral(
        "if(p.length)Promise.all(p).then(restoreScroll);else restoreScroll();");
    html.replace(chainHook, chainHook + QStringLiteral("if(window.scribaPaginate){window.scribaPaginate();}"));

    // The initial DOMContentLoaded render ends with scribaHideOverlay(); same
    // treatment so the very first page-break mode paint is paginated.
    const QString overlayHook = QStringLiteral(
        "var scribaHideOverlay=function(){scribaEndRender();};");
    html.replace(overlayHook, QStringLiteral(
        "var scribaHideOverlay=function(){scribaEndRender();if(window.scribaPaginate){window.scribaPaginate();}};"));

    return html;
}

} // namespace PreviewPagination
