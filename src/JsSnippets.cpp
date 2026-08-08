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
#include "StaticHelpers.h"

const QString mermaidInitJs = QStringLiteral(
    "function initMermaid(){"
    "var els=document.querySelectorAll('code.language-mermaid');"
    "if(!els.length)return Promise.resolve();"
    "els.forEach(function(el){"
    "var pre=el.parentElement;"
    "var line=pre&&pre.getAttribute('data-line')?pre.getAttribute('data-line'):'';"
    "var wrap=document.createElement('div');"
    "wrap.className='scriba-chart-wrap';"
    "if(line)wrap.setAttribute('data-line',line);"
    "pre.parentElement.replaceChild(wrap,pre);"
    "var div=document.createElement('div');"
    "div.className='mermaid';"
    "div.textContent=el.textContent;"
    "wrap.appendChild(div);"
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
const QString anchorNavJs = QString(
        "function scribaScrollToSlug(frag){"
        "try{"
        "var slug=decodeURIComponent(frag).toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/^-+|-+$/g,'');"
        "var el=slug?document.getElementById(slug):null;"
        "if(el){el.scrollIntoView({block:'start',behavior:'auto'});return true;}"
        "}catch(e){}"
        "return false;"
        "}"
        "function scribaScrollToSlugRetry(frag){var tries=0;(function poll(){"
        "if(scribaScrollToSlug(frag)||tries++>20)return;setTimeout(poll,%1);"
        "})();}"
    ).arg(QString::number(JsTiming::AnchorNavRetry));

const QString katexInitJs = QStringLiteral(
    "function scribaRenderMath(){"
    "if(typeof katex==='undefined')return;"
    "document.querySelectorAll('span.katex[data-tex]').forEach(function(el){"
    "if(el.getAttribute('data-rendered'))return;"
    "var disp=el.parentElement&&el.parentElement.classList.contains('katex-display');"
    "try{"
    "katex.render(el.getAttribute('data-tex'),el,{displayMode:!!disp,throwOnError:false});"
    "}catch(e){"
    "el.textContent=el.getAttribute('data-tex');"
    "}"
    "el.setAttribute('data-rendered','1');"
    "});"
    "}"
    "function initKaTeX(){"
    "if(typeof katex==='undefined')return;"
    "scribaMathEditPass();"
    "scribaRenderMath();"
    "}"
);

const QString echartsInitJs = QString(
    "function initECharts(){"
    "var els=document.querySelectorAll('code.language-ec');"
    "if(!els.length)return Promise.resolve();"
    "return Promise.all(Array.from(els).map(function(el){"
    "try{"
    "var spec=JSON.parse(el.textContent);"
    "var container=el.parentElement;"
    "var line=container&&container.getAttribute('data-line')?container.getAttribute('data-line'):'';"
    "var wrap=document.createElement('div');"
    "wrap.className='scriba-chart-wrap';"
    "if(line)wrap.setAttribute('data-line',line);"
    "container.parentElement.replaceChild(wrap,container);"
    "var div=document.createElement('div');"
    "div.className='echarts-chart';"
    "div.style.width='100%';"
    "div.style.minHeight='300px';"
    "div.style.overflow='visible';"
    "wrap.appendChild(div);"
    "return new Promise(function(resolve){"
    "var tries=0;"
    "(function go(){"
    "if(div.clientWidth>0||++tries>%1){"
    "var chart=echarts.init(div,null,{renderer:'svg'});"
    "try{chart.setOption(spec);}catch(e){}"
    "chart.on('finished',function(){resolve(chart);});"
    "setTimeout(function(){resolve(chart);},%2);"
    "}else{setTimeout(go,%3);}"
    "})();"
    "});"
    "}"
    "catch(e){return Promise.resolve();}"
    "}));"
    "}"
    ).arg(QString::number(JsTiming::ChartLayoutTries),
          QString::number(JsTiming::EChartsReadyTimeout),
          QString::number(JsTiming::ChartLayoutPoll));

const QString chartEditJs = QStringLiteral(
    "function scribaEditAnchor(kind,line,idx,tex){"
    "var a=document.createElement('a');"
    "a.className='scriba-edit-btn';"
    "a.href='#scriba-edit:'+kind+':'+line+':'+idx+(tex?':'+encodeURIComponent(tex):'');"
    "a.textContent='\\u270e';"
    "a.title='Edit';"
    "return a;"
    "}"
    "function scribaMathEditPass(){"
    "var content=document.getElementById('scriba-content');"
    "if(!content)return;"
    "var lastLine=-1,idxInLine=0;"
    "Array.prototype.slice.call(content.querySelectorAll('span.katex[data-tex]')).forEach(function(el){"
    "if(el.getAttribute('data-editbox'))return;"
    "el.setAttribute('data-editbox','1');"
    "var lineS=el.getAttribute('data-line');"
    "var line=lineS?parseInt(lineS,10):0;"
    "if(line!==lastLine){lastLine=line;idxInLine=0;}"
    "var disp=el.parentElement&&el.parentElement.classList.contains('katex-display');"
    "var box=document.createElement('span');"
    "box.className='scriba-mathbox';"
    "box.style.display=disp?'block':'inline-block';"
    "el.parentNode.insertBefore(box,el);"
    "box.appendChild(el);"
    "box.appendChild(scribaEditAnchor('math',line,idxInLine++,el.getAttribute('data-tex')));"
    "});"
    "}"
);

const QString setImgTitlesJs = QStringLiteral(
    "function setImgTitles(){"
    "document.querySelectorAll('img:not([title])').forEach(function(img){"
    "if(img.alt)img.title=img.alt;"
    "});"
    "}"
);

// Footnote reference links get the text of their definition as a native title
// tooltip, so hovering `<sup><a href="#fn-N">` shows the note. The definition
// is cloned and its backref (`↶`) removed before reading the text.
const QString setFootnoteTitlesJs = QStringLiteral(
    "function setFootnoteTitles(){"
    "document.querySelectorAll('a[href^=\"#fn-\"]').forEach(function(a){"
    "var m=/^#fn-(\\d+)$/.exec(a.getAttribute('href'));"
    "if(!m)return;"
    "var li=document.getElementById('fn-'+m[1]);"
    "if(!li)return;"
    "var cl=li.cloneNode(true);"
    "cl.querySelectorAll('.footnote-backref').forEach(function(b){b.remove();});"
    "var t=(cl.innerText||cl.textContent||'').replace(/\\s+/g,' ').replace(/^\\s+|\\s+$/g,'');"
    "if(t)a.title=t;"
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

// Lightbox for preview image links. scribaShowImage() renders `href` in a
// full-window overlay; a backdrop click, the × button, or Escape closes it.
// The overlay lives outside #scriba-content, so live re-renders leave it
// open. Displaying the file as an <img> is safe even for SVG: scripts do not
// run in an image context.
const QString imageOverlayJs = QStringLiteral(
    "function scribaShowImage(href){"
    "var ov=document.getElementById('scriba-image-overlay');"
    "if(!ov){"
    "ov=document.createElement('div');"
    "ov.id='scriba-image-overlay';"
    "var box=document.createElement('div');"
    "box.className='scriba-image-box';"
    "var img=document.createElement('img');"
    "img.className='scriba-image-view';"
    "img.alt='';"
    "var cap=document.createElement('div');"
    "cap.className='scriba-image-caption';"
    "var close=document.createElement('button');"
    "close.type='button';"
    "close.className='scriba-image-close';"
    "close.title='Close (Esc)';"
    "close.textContent='\\u00d7';"
    "box.appendChild(img);"
    "box.appendChild(cap);"
    "box.appendChild(close);"
    "ov.appendChild(box);"
    "document.body.appendChild(ov);"
    "ov.addEventListener('click',function(e){if(e.target===ov)scribaHideImage();});"
    "close.addEventListener('click',function(e){e.stopPropagation();scribaHideImage();});"
    "document.addEventListener('keydown',function(e){if(e.key==='Escape')scribaHideImage();});"
    "}"
    "var img=ov.querySelector('img');"
    "img.src=href;"
    "var name=href;"
    "try{name=decodeURIComponent(href.split('/').pop()||href);}catch(e){}"
    "ov.querySelector('.scriba-image-caption').textContent=name;"
    "ov.style.display='flex';"
    "}"
    "function scribaHideImage(){"
    "var ov=document.getElementById('scriba-image-overlay');"
    "if(ov)ov.style.display='none';"
    "}"
);
