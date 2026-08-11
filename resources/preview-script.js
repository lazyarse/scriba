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

function scribaUpdate(html, themeCss, mermaidTheme, emojiMode, delay, baseUrl) {
    if (!document.body) return false;
    try {
        window._scribaGen = (window._scribaGen || 0) + 1;
        var gen = window._scribaGen;
        var sy = window.scrollY;
        var sh = document.body.scrollHeight;
        var ih = window.innerHeight;
        var pct = sh > ih ? sy / (sh - ih) : 0;
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
        var sc = document.getElementById('scriba-content');
        if (sc) sc.innerHTML = html;
        else return false;
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
                if (Math.abs(window.scrollY - sy) < 2) {
                    var ih2 = window.innerHeight;
                    window.scrollTo(0, pct * Math.max(1, document.body.scrollHeight - ih2));
                }
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
        scribaEndRender();
    };
    if (p.length) Promise.all(p).then(scribaHideOverlay, scribaHideOverlay);
    else scribaHideOverlay();
    setTimeout(scribaHideOverlay, 10000);
});
