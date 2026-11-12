/* Binding/levy-wasm.js
   Load order in frontend/index.html:
     <script src="../Binding/levy_wasm.js" defer></script>
     <script src="../Binding/levy-wasm.js" defer></script>
     <script src="app.js" defer></script>
*/
window.LevyWasm = (() => {
  // Each file: where C++ expects it (dest, under the virtual data/ folder) and
  // browser URLs to try in order. URLs are relative to frontend/index.html.
  const FILES = [
    { dest: 'levy_training.csv', required: true,
      urls: ['../data/levy_training.csv', '../data/processed/levy_training.csv'] },
    { dest: 'raw/petrol_price_buildup_fortnightly.csv', required: true,
      urls: ['../data/raw/petrol_price_buildup_fortnightly.csv', '../data/petrol_price_buildup_fortnightly.csv'] },
    { dest: 'raw/cpi_and_income_monthly.csv', required: true,
      urls: ['../data/raw/cpi_and_income_monthly.csv', '../data/cpi_and_income_monthly.csv'] },
    { dest: 'processed/brent_daily.csv', required: false,
      urls: ['../data/processed/brent_daily.csv', '../data/raw/brent_daily.csv'] },
  ];

  let ready = null;

  async function fetchText(urls) {
    const tried = [];
    for (const url of urls) {
      try {
        const r = await fetch(url);
        if (!r.ok) { tried.push(url + ' (' + r.status + ')'); continue; }
        const text = await r.text();
        if (/^\s*</.test(text)) { tried.push(url + ' (got HTML, not CSV)'); continue; }
        return text;
      } catch (e) { tried.push(url + ' (' + e.message + ')'); }
    }
    throw new Error('Could not load: ' + tried.join(', '));
  }

  // C++ opens "../data/..." (tree, advisor) and "data/..." (forecast generator).
  // With cwd = /work, write every file to both /data/... and /work/data/...
  function mount(M, name, text) {
    for (const root of ['/data', '/work/data']) {
      const full = root + '/' + name;
      M.FS.mkdirTree(full.slice(0, full.lastIndexOf('/')));
      M.FS.writeFile(full, text);
    }
  }

  function init() {
    if (ready) return ready;
    ready = (async () => {
      if (typeof createLevyModule !== 'function')
        throw new Error('createLevyModule is missing. Load ../Binding/levy_wasm.js before levy-wasm.js.');
      const M = await createLevyModule();
      M.FS.mkdirTree('/work');
      M.FS.chdir('/work');
      for (const f of FILES) {
        try { mount(M, f.dest, await fetchText(f.urls)); }
        catch (e) { if (f.required) throw e; console.warn('[levy-wasm] optional file skipped:', e.message); }
      }
      return M;
    })();
    ready.catch(() => { ready = null; });   // allow retry after a failed load
    return ready;
  }

  // A C++ abort (e.g. exception without -fexceptions) kills the module: drop it so the next call remounts.
  async function run(fn) {
    const M = await init();
    try { return fn(M); }
    catch (e) {
      if (e instanceof WebAssembly.RuntimeError || /abort/i.test(String(e))) ready = null;
      throw e;
    }
  }

  const BAND = { LOW: 'low', MEDIUM: 'moderate', HIGH: 'high' };
  const pick = (re, s) => (s.match(re) || [])[1];

  function parseRecommendation(res) {
    const out = res.output;
    if (res.code !== 0) return { ok: false, error: out.trim() || 'C++ returned code ' + res.code, output: out };
    const levy = parseFloat(pick(/Recommended levy:\s+(-?[\d.]+)/, out));
    const bandName = pick(/Recommended band:\s+(\w+)/, out);
    const ok = Number.isFinite(levy) && bandName in BAND;
    return {
      ok, levy, band: BAND[bandName] || null, bandName,
      multiplier: parseFloat(pick(/Predicted multiplier:\s+(-?[\d.]+)/, out)),
      requiredLevy: parseFloat(pick(/Required levy:\s+(-?[\d.]+)/, out)),
      periodStart: pick(/(?:Period Start|Target Date):\s+(\S+)/, out),
      output: out,
      error: ok ? undefined : 'Could not read the recommendation from C++ output: ' + out.trim(),
    };
  }

  return {
    ready: init(),
    live: crude => run(M => parseRecommendation(M.runLive(Number(crude)))),
    backtest: date => run(M => parseRecommendation(M.runBacktest(String(date)))),
    report: () => run(M => {
      const r = M.runReport();
      return { ok: r.code === 0, output: r.output, error: r.code === 0 ? undefined : r.output.trim() };
    }),
    // Mean ARIMA forecast of the next nDays; -1 if fewer than 250 points or no model fits.
    arimaMean: (prices, nDays = 10) => run(M => {
      const v = new M.VectorDouble();
      try { for (const p of prices) v.push_back(p); return M.forecastMean(v, nDays); }
      finally { v.delete(); }
    }),
    generateForecastCol: () => run(M => {
      const r = M.generateForecastCol();
      if (r.code !== 0) return { ok: false, log: r.output, rows: [] };
      const csv = M.FS.readFile('/work/data/processed/forecast_col.csv', { encoding: 'utf8' });
      const rows = csv.trim().split('\n').slice(1).map(l => {
        const [period_start, v] = l.split(',');
        return { period_start, forecast_crude_price: v === '' ? null : parseFloat(v) };
      });
      return { ok: true, csv, rows, log: r.output };
    }),
  };
})();