/* levy-wasm.js — JS bridge to the C++ ARIMA + RegressionTree code.
   Load AFTER levy_wasm.js (the Emscripten output) and BEFORE app.js:
     <script src="levy_wasm.js"></script>
     <script src="levy-wasm.js"></script>
*/
const LevyWasm = (() => {
  const BASE_URL = new URL('../data/', document.currentScript.src);   // where the browser fetches the CSVs from
  // [file under BASE_URL, path inside the virtual "data/" folder]
  const DATA_FILES = [
    ['processed/levy_training.csv',                       'levy_training.csv'],
    ['processed/brent_daily.csv',                         'processed/brent_daily.csv'],
    ['raw/petrol_price_buildup_fortnightly.csv',    'raw/petrol_price_buildup_fortnightly.csv'],
    ['raw/cpi_and_income_monthly.csv',              'raw/cpi_and_income_monthly.csv'],
  ];

  let mod = null, ready = null;

  // Your C++ opens both "../data/..." (tree) and "data/..." (forecast generator),
  // so each file is written to /work/data/... AND /data/... with cwd = /work.
  function mount(M, name, text) {
    for (const root of ['/work/data', '/data']) {
      const full = `${root}/${name}`;
      M.FS.mkdirTree(full.substring(0, full.lastIndexOf('/')));
      M.FS.writeFile(full, text);
    }
  }

  function init() {
    if (ready) return ready;
    ready = (async () => {
      const M = await createLevyModule();
      M.FS.mkdirTree('/work');
      M.FS.chdir('/work');
      await Promise.all(DATA_FILES.map(async ([url, name]) => {
        const r = await fetch(new URL(url, BASE_URL));
        if (!r.ok) throw new Error(`Could not load ${BASE_URL + url} (${r.status})`);
        mount(M, name, await r.text());
      }));
      mod = M;
      return M;
    })().catch(error => {
      ready = null;
      throw error;
    });
    return ready;
  }

  const bandSlug = b => ({ LOW: 'low', MEDIUM: 'moderate', HIGH: 'high' }[b] || 'low');
  const pick = (re, s) => (s.match(re) || [])[1];

  function parseRecommendation(res) {
    const out = res.output;
    if (res.code !== 0) return { ok: false, error: out.trim(), output: out };
    const levy = parseFloat(pick(/Recommended levy:\s+(-?[\d.]+)/, out));
    const band = pick(/Recommended band:\s+(\w+)/, out);
    return {
      ok: Number.isFinite(levy),
      levy,
      band: bandSlug(band),
      bandName: band,
      multiplier: parseFloat(pick(/Predicted multiplier:\s+(-?[\d.]+)/, out)),
      requiredLevy: parseFloat(pick(/Required levy:\s+(-?[\d.]+)/, out)),
      periodStart: pick(/(?:Period Start|Target Date):\s+(\S+)/, out),
      output: out,
    };
  }

  // Live recommendation for a forecast crude price (USD/bbl)
  async function live(crude) {
    const M = await init();
    return parseRecommendation(M.runLive(Number(crude)));
  }

  // Backtest a date that exists in levy_training.csv (YYYY-MM-DD)
  async function backtest(date) {
    const M = await init();
    return parseRecommendation(M.runBacktest(String(date)));
  }

  // Full text report (accuracy, baselines, learned tree)
  async function report() {
    const M = await init();
    const r = M.runReport();
    return { ok: r.code === 0, output: r.output };
  }

  // ARIMA mean forecast over the next nDays for an array of daily prices.
  // Returns -1 if there is too little history (< 250 points) or no model fits.
  async function arimaMean(prices, nDays = 10) {
    const M = await init();
    const v = new M.VectorDouble();
    try {
      for (const p of prices) v.push_back(p);
      return M.forecastMean(v, nDays);
    } finally { v.delete(); }
  }

  // Re-generates forecast_col.csv from brent_daily.csv. Slow (runs ARIMA per row).
  async function generateForecastCol() {
    const M = await init();
    const r = M.generateForecastCol();
    if (r.code !== 0) return { ok: false, log: r.output, rows: [] };
    const csv = M.FS.readFile('/work/data/processed/forecast_col.csv', { encoding: 'utf8' });
    const rows = csv.trim().split('\n').slice(1).map(l => {
      const [period_start, v] = l.split(',');
      return { period_start, forecast_crude_price: v === '' ? null : parseFloat(v) };
    });
    return { ok: true, csv, rows, log: r.output };
  }

  // Forecast plus metrics from a separate chronological holdout, in price units.
  async function arimaReport(prices, nDays = 12) {
    const M = await init();
    const v = new M.VectorDouble();
    try {
      for (const price of prices) v.push_back(price);
      return M.forecastWithEvaluation(v, nDays);
    } finally { v.delete(); }
  }
  return { init, live, backtest, report, arimaMean, arimaReport, generateForecastCol };
})();

/* ---- Wiring into app.js -------------------------------------------------
   In runLevy(), replace the line  `renderLevyResult();`  with:

   try {
     const latest = [...(await (await fetch('./data/forecast_col.csv')).text())
       .trim().split('\n').slice(1)].map(l => parseFloat(l.split(',')[1])).filter(Number.isFinite).pop();
     const r = await LevyWasm.live(latest);
     const levy = PREDICTIONS.find(p => p.label === 'Petroleum levy');
     if (r.ok && levy) { levy.forecast = r.levy; levy.band = r.band; levy.targetDate = r.periodStart; }
   } catch (e) { console.error(e); }
   renderLevyResult();
*/
