function twemojiParse(m) {
    if (m === 'color' && typeof twemoji !== 'undefined') {
        twemoji.parse(document.body, {
            base: 'qrc:///twemoji/',
            folder: 'svg',
            ext: '.svg',
            className: 'emoji'
        });
    }
}

function scribaDocTop(el){ return el.getBoundingClientRect().top + window.scrollY; }
function scribaRebuildAnchorIndex(){
    window._scribaAnchors=[];
    var cs=document.getElementById('scriba-content');
    if(!cs) return;
    var els=cs.querySelectorAll('[data-line]');
    for(var i=0;i<els.length;i++){
        var lv=els[i].getAttribute('data-line');
        if(!lv) continue;
        window._scribaAnchors.push({line:parseFloat(lv),el:els[i]});
    }
    window._scribaAnchors.sort(function(a,b){return a.line-b.line;});
}
function scribaScrollToSourceLine(line){
    line=parseFloat(line);
    var a=window._scribaAnchors||[];
    if(!a.length)return;
    var lo=0,hi=a.length-1,idx=-1;
    while(lo<=hi){var mid=(lo+hi)>>1;if(a[mid].line<=line){idx=mid;lo=mid+1;}else hi=mid-1;}
    if(idx<0){window.scrollTo(0,0);return;}
    var A=a[idx];
    if(idx+1>=a.length){window.scrollTo(0,document.body.scrollHeight);return;}
    var B=a[idx+1];
    var t=(B.line===A.line)?0:Math.min(1,Math.max(0,(line-A.line)/(B.line-A.line)));
    window.scrollTo(0,scribaDocTop(A.el)+(scribaDocTop(B.el)-scribaDocTop(A.el))*t);
}
function scribaCaptureAnchorLine(){
    var a=window._scribaAnchors||[];
    if(!a.length)return 0;
    var sy=window.scrollY;
    var lo=0,hi=a.length-1,idx=-1;
    while(lo<=hi){var mid=(lo+hi)>>1;if(scribaDocTop(a[mid].el)<=sy+2){idx=mid;lo=mid+1;}else hi=mid-1;}
    if(idx<0)return a[0].line;
    var A=a[idx];
    if(idx+1>=a.length)return A.line;
    var B=a[idx+1];
    var dA=scribaDocTop(A.el),dB=scribaDocTop(B.el);
    var t=(dB-dA)<=0?0:Math.min(1,Math.max(0,(sy-dA)/(dB-dA)));
    return A.line+(B.line-A.line)*t;
}
function scribaUpdate(html, themeCss, mermaidTheme, emojiMode, delay, baseUrl, tabSwitch) {
    if (!document.body) return false;
    try {
        window._scribaGen = (window._scribaGen || 0) + 1;
        var gen = window._scribaGen;
        var sy = window.scrollY;
        if (themeCss) {
            var tc = document.getElementById('theme-css');
            if (tc) tc.textContent = themeCss;
        }
        if (baseUrl) {
            var b = document.getElementById('scriba-base');
            if (!b) {
                b = document.createElement('base');
                b.id = 'scriba-base';
                var hd = document.head;
                hd.insertBefore(b, hd.firstChild);
            }
            b.href = baseUrl;
        } else {
            var b2 = document.getElementById('scriba-base');
            if (b2) b2.remove();
        }
        window._scribaBasePath = baseUrl ? new URL(baseUrl).pathname : location.pathname;
        var anchorLine = tabSwitch ? null : scribaCaptureAnchorLine();
        var sc = document.getElementById('scriba-content');
        if (sc) sc.innerHTML = html;
        else return false;
        window._scribaAnchors = [];
        clearTimeout(window._scribaHeavyTimer);
        window._scribaHeavyTimer = setTimeout(function () {
            if (gen !== window._scribaGen) return;
            mermaid.initialize({startOnLoad: false, theme: mermaidTheme});
            var mp = initMermaid();
            initKaTeX();
            var vp = initECharts();
            hljs.highlightAll();
            generateHeadingIds();
            setImgTitles();
            setFootnoteTitles();
            replaceEmoji(document.body);
            twemojiParse(emojiMode);
            function restoreScroll() {
                var userScrolled = Math.abs(window.scrollY - sy) >= 2;
                if (window.scribaPaginate) window.scribaPaginate();
                if (anchorLine != null && !userScrolled) scribaScrollToSourceLine(anchorLine);
            }
            var p = [];
            if (typeof mp !== 'undefined') p.push(mp);
            if (typeof vp !== 'undefined') p.push(vp);
            var imgs = document.querySelectorAll('img:not(.emoji)');
            if (imgs.length > 0) {
                p.push(new Promise(function (r) {
                    var n = 0, t = imgs.length;
                    function c() {
                        n++;
                        if (n >= t) r();
                    }
                    for (var i = 0; i < imgs.length; i++) {
                        if (imgs[i].complete) c();
                        else {
                            imgs[i].onload = c;
                            imgs[i].onerror = c;
                        }
                    }
                }));
            }
            scribaRebuildAnchorIndex();
            if (p.length) Promise.all(p).then(restoreScroll);
            else restoreScroll();
        }, (typeof delay === 'number' && delay >= 0) ? delay : window._scribaHeavyDelay);
        return true;
    } catch (e) {
        scribaShowRenderError(e && e.message ? e.message : e);
        scribaEndRender();
        return false;
    }
}

function scribaBeginRender() {
    var c = document.getElementById('scriba-content');
    if (c) c.innerHTML = '';
    var o = document.getElementById('scriba-rendering-overlay');
    if (!o && document.body) {
        o = document.createElement('div');
        o.id = 'scriba-rendering-overlay';
        o.textContent = 'Rendering…';
        document.body.insertBefore(o, document.body.firstChild);
    }
    if (o) o.style.display = 'flex';
}

function scribaEndRender() {
    var o = document.getElementById('scriba-rendering-overlay');
    if (o) o.style.display = 'none';
}

function scribaShowRenderError(m) {
    var c = document.getElementById('scriba-content');
    if (!c && document.body) {
        c = document.createElement('div');
        c.id = 'scriba-content';
        document.body.appendChild(c);
    }
    if (!c) return;
    m = String(m == null ? 'Unknown render error' : m);
    var t = document.createElement('div');
    t.style.cssText = 'margin:2rem auto;max-width:720px;padding:1.2rem 1.4rem;border:1px solid #d33;border-radius:6px;background:#fdf0f0;color:#8b0000;font-family:system-ui,sans-serif;';
    t.innerHTML = '<strong>Preview error</strong><pre style="white-space:pre-wrap;word-break:break-word;font-family:monospace;margin:0.5rem 0 0;color:#6b0000;">' + String(m).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;') + '</pre>';
    c.innerHTML = '';
    c.appendChild(t);
}

document.addEventListener('DOMContentLoaded', function () {
    window._scribaBasePath = location.pathname;
    mermaid.initialize({startOnLoad: false, theme: '{{MERMAID_THEME}}'});
    hljs.registerAliases('ec', {languageName: 'json'});
    hljs.highlightAll();
    generateHeadingIds();
    initKaTeX();
    setImgTitles();
    setFootnoteTitles();
    replaceEmoji(document.body);
    twemojiParse('{{EMOJI_MODE}}');
    var p = [];
    var mp = window.mermaidReady = initMermaid();
    var vp = window.echartsReady = initECharts();
    if (typeof mp !== 'undefined') p.push(mp);
    if (typeof vp !== 'undefined') p.push(vp);
    var imgs = document.querySelectorAll('img:not(.emoji)');
    if (imgs.length > 0) {
        p.push(new Promise(function (r) {
            var n = 0, t = imgs.length;
            function c() {
                n++;
                if (n >= t) r();
            }
            for (var i = 0; i < imgs.length; i++) {
                if (imgs[i].complete) c();
                else {
                    imgs[i].onload = c;
                    imgs[i].onerror = c;
                }
            }
        }));
    }
    var scribaHideOverlay = function () {
        scribaRebuildAnchorIndex();
        scribaEndRender();
        if (window.scribaPaginate) window.scribaPaginate();
    };
    if (p.length) Promise.all(p).then(scribaHideOverlay, scribaHideOverlay);
    else scribaHideOverlay();
    setTimeout(scribaHideOverlay, 10000);
});
