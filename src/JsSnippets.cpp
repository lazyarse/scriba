#include "JsSnippets.h"

const QString mermaidInitJs = QStringLiteral(
    "function initMermaid(){"
    "var els=document.querySelectorAll('code.language-mermaid');"
    "if(!els.length)return;"
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
    "document.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(function(h){"
    "if(!h.id){"
    "h.id=h.textContent.toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/^-+|-+$/g,'');"
    "}"
    "});"
    "}"
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
    "return vegaEmbed(div,spec,{actions:false}).catch(function(){});"
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
