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
#include "JsSnippets.h"

const QString mermaidInitJs = QStringLiteral(
    "function initMermaid(){"
    "var els=document.querySelectorAll('code.language-mermaid');"
    "if(!els.length)return Promise.resolve();"
    "els.forEach(function(el){"
    "var div=document.createElement('div');"
    "div.className='mermaid';"
    "div.textContent=el.textContent;"
    "el.parentElement.parentElement.replaceChild(div,el.parentElement);"
    "});"
    "return mermaid.run({querySelector:'.mermaid'});"
    "}"
);

const QString headingIdJs = QStringLiteral(
    "function generateHeadingIds(){"
    "var used={};"
    "document.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(function(h){"
    "if(!h.id){"
    "var base=h.textContent.toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/^-+|-+$/g,'');"
    "if(!base)return;"
    "var id=base,n=1;while(used[id])id=base+'-'+(n++);used[id]=true;h.id=id;"
    "}"
    "});"
    "}"
);

// Anchor navigation for `#heading` links. scribaScrollToSlug() slugifies the
// fragment the way generateHeadingIds() slugs headings and scrolls to the
// match; it returns whether the element was found. scribaScrollToSlugRetry()
// polls for up to ~6s (headings get their ids only after the heavy render
// pass) — safe for same-document jumps, where no page reload kills the
// closure. Cross-document jumps retry from C++ instead (a fresh page load
// discards in-flight JS).
const QString anchorNavJs = QStringLiteral(
    "function scribaScrollToSlug(frag){"
    "try{"
    "var slug=decodeURIComponent(frag).toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/^-+|-+$/g,'');"
    "var el=slug?document.getElementById(slug):null;"
    "if(el){el.scrollIntoView({block:'start',behavior:'auto'});return true;}"
    "}catch(e){}"
    "return false;"
    "}"
    "function scribaScrollToSlugRetry(frag){var tries=0;(function poll(){"
    "if(scribaScrollToSlug(frag)||tries++>20)return;setTimeout(poll,300);"
    "})();}"
);

const QString katexInitJs = QStringLiteral(
    "function initKaTeX(){"
    "if(typeof renderMathInElement==='function')"
    "renderMathInElement(document.body,{"
    "delimiters:["
    "{left:'$$',right:'$$',display:true},"
    "{left:'$',right:'$',display:false}"
    "]"
    "});"
    "}"
);

const QString vegaLiteInitJs = QStringLiteral(
    "function initVegaLite(){"
    "var els=document.querySelectorAll('code.language-vl');"
    "if(!els.length)return Promise.resolve();"
    "return Promise.all(Array.from(els).map(function(el){"
    "try{"
    "var spec=JSON.parse(el.textContent);"
    "var container=el.parentElement;"
    "var div=document.createElement('div');"
    "div.className='vega-lite-chart';"
    "div.style.width='100%';"
    "div.style.minHeight='300px';"
    "div.style.overflow='visible';"
    "container.parentElement.replaceChild(div,container);"
    "return new Promise(function(resolve){"
    "var tries=0;"
    "(function go(){"
    "if(div.clientWidth>0||++tries>40){"
    "resolve(vegaEmbed(div,spec,{actions:false,renderer:'svg'}).catch(function(){}));"
    "}else{setTimeout(go,50);}"
    "})();"
    "});"
    "}"
    "catch(e){return Promise.resolve();}"
    "}));"
    "}"
);

const QString setImgTitlesJs = QStringLiteral(
    "function setImgTitles(){"
    "document.querySelectorAll('img:not([title])').forEach(function(img){"
    "if(img.alt)img.title=img.alt;"
    "});"
    "}"
);

const QString katexToImageJs = QStringLiteral(
    "function convertKatexToImages(){"
    "var elements=document.querySelectorAll('.katex');"
    "if(!elements.length)return Promise.resolve();"
    "var cssText='';"
    "try{"
    "for(var si=0;si<document.styleSheets.length;si++){"
    "try{"
    "var rules=document.styleSheets[si].cssRules;"
    "for(var ri=0;ri<rules.length;ri++){"
    "var s=rules[ri].selectorText||'';"
    "if(s&&s.indexOf('katex')!==-1){"
    "cssText+=rules[ri].cssText+'\\n';"
    "}"
    "}"
    "}catch(e2){}"
    "}"
    "}catch(e1){}"
    "var promises=[];"
    "elements.forEach(function(el){"
    "var promise=new Promise(function(resolve){"
    "try{"
    "var rect=el.getBoundingClientRect();"
    "if(rect.width<1||rect.height<1){"
    "var span=document.createElement('span');"
    "span.textContent=el.textContent;"
    "el.parentNode.replaceChild(span,el);"
    "resolve();return;"
    "}"
    "var canvas=document.createElement('canvas');"
    "var scale=2;"
    "canvas.width=rect.width*scale;"
    "canvas.height=rect.height*scale;"
    "var ctx=canvas.getContext('2d');"
    "ctx.scale(scale,scale);"
    "var data='<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"'+rect.width+'\" height=\"'+rect.height+'\">'"
    "+'<foreignObject width=\"100%\" height=\"100%\">'"
    "+'<div xmlns=\"http://www.w3.org/1999/xhtml\">'"
    "+(cssText?'<style>'+cssText+'</style>':'')"
    "+el.outerHTML"
    "+'</div>'"
    "+'</foreignObject></svg>';"
    "var img=new Image();"
    "img.onload=function(){"
    "try{"
    "ctx.drawImage(img,0,0);"
    "var dataUrl=canvas.toDataURL('image/png');"
    "var imgEl=document.createElement('img');"
    "imgEl.src=dataUrl;"
    "imgEl.style.verticalAlign='middle';"
    "el.parentNode.replaceChild(imgEl,el);"
    "resolve();"
    "}catch(e){"
    "var span=document.createElement('span');"
    "span.textContent=el.textContent;"
    "el.parentNode.replaceChild(span,el);"
    "resolve();"
    "}"
    "};"
    "img.onerror=function(){"
    "var span=document.createElement('span');"
    "span.textContent=el.textContent;"
    "el.parentNode.replaceChild(span,el);"
    "resolve();"
    "};"
    "img.src='data:image/svg+xml;base64,'+btoa(unescape(encodeURIComponent(data)));"
    "}catch(e){"
    "var span=document.createElement('span');"
    "span.textContent=el.textContent;"
    "el.parentNode.replaceChild(span,el);"
    "resolve();"
    "}"
    "});"
    "promises.push(promise);"
    "});"
    "return Promise.all(promises);"
    "}"
);
