/* Feather PDF landing page. No dependencies, no tracking. */
(function () {
  'use strict';
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  /* ── Scroll reveal ─────────────────────────────────────────────────── */
  var revealEls = document.querySelectorAll('.reveal');
  if (!reduced && 'IntersectionObserver' in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { e.target.classList.add('in'); io.unobserve(e.target); }
      });
    }, { threshold: 0.12 });
    revealEls.forEach(function (el) { io.observe(el); });
  } else {
    revealEls.forEach(function (el) { el.classList.add('in'); });
  }

  /* ── Stat counters ─────────────────────────────────────────────────── */
  function runCount(el) {
    var target = parseInt(el.getAttribute('data-count'), 10);
    if (reduced || target === 0) { el.textContent = target; return; }
    var t0 = performance.now(), dur = 1100;
    function tick(now) {
      var p = Math.min(1, (now - t0) / dur);
      el.textContent = Math.round(target * (1 - Math.pow(1 - p, 3)));
      if (p < 1) requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
  }
  var nums = document.querySelectorAll('.stat-num');
  if ('IntersectionObserver' in window) {
    var ioNum = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { runCount(e.target); ioNum.unobserve(e.target); }
      });
    }, { threshold: 0.6 });
    nums.forEach(function (el) { ioNum.observe(el); });
  } else {
    nums.forEach(runCount);
  }

  /* ── Expandable feature cards ──────────────────────────────────────── */
  document.querySelectorAll('.card-head').forEach(function (btn) {
    btn.addEventListener('click', function () {
      var card = btn.closest('.card');
      var open = card.classList.toggle('open');
      btn.setAttribute('aria-expanded', open ? 'true' : 'false');
    });
  });

  /* ── Find & Redact demo ────────────────────────────────────────────── */
  var redactBtn = document.getElementById('redactBtn');
  var redactStatus = document.getElementById('redactStatus');
  var statusDefault = redactStatus ? redactStatus.textContent : '';
  if (redactBtn) redactBtn.addEventListener('click', function () {
    var pii = document.querySelectorAll('#redactText .pii');
    if (redactBtn.getAttribute('data-state') === 'done') {
      pii.forEach(function (s) { s.classList.remove('found', 'redacted'); });
      redactBtn.setAttribute('data-state', '');
      redactBtn.innerHTML = 'Find &amp; redact';
      redactStatus.textContent = statusDefault;
      return;
    }
    redactBtn.disabled = true;
    redactStatus.textContent = 'Scanning for email, phone, and card patterns…';
    var findDelay = reduced ? 0 : 350;
    pii.forEach(function (s, i) {
      setTimeout(function () { s.classList.add('found'); }, findDelay * (i + 1));
    });
    setTimeout(function () {
      pii.forEach(function (s, i) {
        setTimeout(function () { s.classList.add('redacted'); }, reduced ? 0 : i * 220);
      });
      setTimeout(function () {
        redactStatus.textContent = pii.length + ' matches redacted. In the app this rasterises the page: the text underneath is removed, not hidden.';
        redactBtn.innerHTML = '<i class="fa-solid fa-rotate-left"></i> Reset';
        redactBtn.setAttribute('data-state', 'done');
        redactBtn.disabled = false;
      }, reduced ? 50 : pii.length * 220 + 350);
    }, findDelay * (pii.length + 1) + (reduced ? 50 : 500));
  });

  /* ── Terminal demo (real commands, real output formats) ───────────── */
  var termBody = document.getElementById('termBody');
  if (termBody) {
    var SCRIPT = [
      { cmd: 'feather-pdf merge book.pdf intro.pdf chapters.pdf annex.pdf',
        out: 'Merged 3 files into book.pdf' },
      { cmd: 'feather-pdf optimize scan.pdf small.pdf',
        out: 'Optimized small.pdf: 18734211 -> 4102931 bytes (78.1% smaller)' },
      { cmd: 'feather-pdf extract report.pdf pages.pdf --pages 1-3,7',
        out: 'Extracted 4 pages into pages.pdf' },
      { cmd: 'feather-pdf ltv signed.pdf archived.pdf',
        out: 'Added long-term validation to archived.pdf: 3 certificate(s), 1 OCSP, 1 CRL across 1 signature(s)' },
      { cmd: 'feather-pdf watermark draft.pdf out.pdf --text CONFIDENTIAL',
        out: 'Watermarked out.pdf' }
    ];
    var idx = 0;
    var trim = function () {
      while (termBody.children.length > 8) termBody.removeChild(termBody.firstChild);
    };
    var typeLine = function () {
      var item = SCRIPT[idx % SCRIPT.length];
      var line = document.createElement('div');
      line.innerHTML = '<span class="tprompt">$ </span><span class="tcmd"></span><span class="tcaret"></span>';
      termBody.appendChild(line);
      trim();
      var cmdSpan = line.querySelector('.tcmd');
      var caret = line.querySelector('.tcaret');
      var i = 0;
      var finish = function () {
        caret.remove();
        var outLine = document.createElement('div');
        outLine.className = 'tout';
        outLine.textContent = item.out;
        termBody.appendChild(outLine);
        trim();
        idx++;
        setTimeout(typeLine, reduced ? 3200 : 2000);
      };
      var step = function () {
        if (reduced) { cmdSpan.textContent = item.cmd; finish(); return; }
        cmdSpan.textContent = item.cmd.slice(0, ++i);
        if (i < item.cmd.length) setTimeout(step, 26 + Math.random() * 38);
        else setTimeout(finish, 420);
      };
      step();
    };
    typeLine();
  }

  /* ── Signing stepper ───────────────────────────────────────────────── */
  var steps = Array.prototype.slice.call(document.querySelectorAll('.step'));
  var sigDetail = document.getElementById('sigDetail');
  var sigBar = document.getElementById('sigBar');
  if (steps.length && sigDetail) {
    var DETAILS = [
      'Sign with a text or graphical appearance, using an NSS certificate or a PKCS#11 smartcard / YubiKey. This proves who signed.',
      'An RFC 3161 timestamp authority countersigns the signature. This proves when, independently of anyone’s clock.',
      'The certificate chain plus OCSP/CRL responses are embedded into the document’s /DSS as an incremental update, so existing signatures stay byte-for-byte intact. Verifiers no longer need the network, or the CA to still exist.',
      'A /DocTimeStamp covers every byte of the document (PAdES-LTA). Renew it every few years and the file outlives certificates, algorithms, even the timestamp authority itself.'
    ];
    var cur = 0;
    var render = function () {
      steps.forEach(function (s, j) {
        s.classList.toggle('active', j === cur);
        s.classList.toggle('done', j < cur);
      });
      sigDetail.textContent = DETAILS[cur];
      if (sigBar) sigBar.style.width = ((cur + 1) / steps.length * 100) + '%';
    };
    var auto = null;
    if (!reduced) auto = setInterval(function () { cur = (cur + 1) % steps.length; render(); }, 4200);
    steps.forEach(function (s, i) {
      s.addEventListener('click', function () {
        if (auto) { clearInterval(auto); auto = null; }
        cur = i;
        render();
      });
    });
    render();
  }
})();
