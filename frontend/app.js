// DEBUG: replacement app.js. Existing CSS and HTML IDs are preserved.
// DEBUG: requires the existing LevyWasm wrapper and its compiled .wasm file.
(() => {
'use strict';
function boot() {
  // DEBUG: protect against duplicate script tags and initialize only after DOM parsing.
  if (window.__fiscalLensBooted) return;
  window.__fiscalLensBooted = true;
const $ = id => document.getElementById(id);
const number = v => v == null ? '—' : new Intl.NumberFormat('en-PK', { maximumFractionDigits: 2 }).format(v);

let HISTORY = [];
let PREDICTIONS = [];
let BRENT_PRICES = [];
let USD_PRICES = [];

// DEBUG: quoted CSV fields, CRLF, BOM and tab-separated files are supported.
function parseCSV(text) {
  text = text.replace(/^\uFEFF/, '');
  if (/^\s*</.test(text)) throw new Error('Received HTML instead of CSV.');
  const delimiter = text.split(/\r?\n/, 1)[0].includes('\t') ? '\t' : ',';
  const rows = []; let row = [], field = '', quoted = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '"') {
      if (quoted && text[i + 1] === '"') { field += '"'; i++; }
      else quoted = !quoted;
    } else if (c === delimiter && !quoted) { row.push(field.trim()); field = ''; }
    else if (c === '\n' && !quoted) {
      row.push(field.trim()); if (row.some(Boolean)) rows.push(row);
      row = []; field = '';
    } else field += c;
  }
  if (quoted) throw new Error('CSV has an unclosed quoted field.');
  row.push(field.trim()); if (row.some(Boolean)) rows.push(row);
  const headers = rows.shift() || [];
  return rows.map(cols => Object.fromEntries(headers.map((h, i) => [h, cols[i] || ''])));
}
async function loadCSV(paths, columns) {
  const failures = [];
  for (const path of paths) {
    try {
      const res = await fetch(path, { signal: AbortSignal.timeout(15000) });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const rows = parseCSV(await res.text());
      if (!rows.length || !columns.some(key => key in rows[0])) throw new Error('Missing rows or required columns');
      return rows;
    } catch (error) { failures.push(path + ': ' + error.message); }
  }
  throw new Error(failures.join('; '));
}

function renderCards() {
  const cardsEl = $('cards');
  if (!cardsEl) return;
  cardsEl.replaceChildren();
  PREDICTIONS.forEach(p => {
    const card = document.createElement('article'); card.className = 'card';
    const heading = document.createElement('div'), value = document.createElement('div');
    heading.className = 'reference-heading'; value.className = 'reference-value';
    const label = document.createElement('h3'); label.textContent = p.label;
    const date = document.createElement('p'); date.className = 'small-note';
    date.textContent = p.forecast == null ? (p.date || '—') : (p.targetDate || '—');
    heading.append(label, date);
    const metric = document.createElement('div'); metric.className = 'metric';
    metric.textContent = number(p.forecast ?? p.current);
    const unit = document.createElement('p'); unit.className = 'unit'; unit.textContent = p.unit;
    const status = document.createElement('span'); status.className = 'small-note';
    status.textContent = p.forecast == null ? 'Historical' : 'Forecast';
    value.append(metric, unit, status); card.append(heading, value); cardsEl.append(card);
  });
}

const themeBtn = $('theme-toggle');
function setTheme(t, animate) {
  const apply = () => {
    document.documentElement.dataset.theme = t;
    const themeAction = t === 'light' ? 'Switch to dark mode' : 'Switch to light mode';
    if (themeBtn) {
      themeBtn.setAttribute('aria-label', themeAction);
      themeBtn.title = themeAction;
    }
    const metaTheme = document.querySelector('meta[name="theme-color"]');
    if (metaTheme) {
      metaTheme.content = getComputedStyle(document.documentElement).getPropertyValue('--bg').trim();
    }
    try { localStorage.setItem('theme', t); } catch (e) {}
  };
  if (animate && document.startViewTransition) document.startViewTransition(apply); else apply();
}
if (themeBtn) {
  themeBtn.addEventListener('click', () => setTheme(document.documentElement.dataset.theme === 'light' ? 'dark' : 'light', true));
}
setTheme(document.documentElement.dataset.theme || 'dark');

const navLinks = [...document.querySelectorAll('.nav a')];
function updateNavigation() {
  if (!navLinks.length) return;
  let current = navLinks.at(0);
  navLinks.forEach(link => {
    const section = document.querySelector(link.getAttribute('href'));
    if (section && section.getBoundingClientRect().top <= 160) current = link;
  });
  if (window.scrollY + window.innerHeight >= document.documentElement.scrollHeight - 3) current = navLinks.at(-1);
  navLinks.forEach(link => {
    const active = link === current;
    link.classList.toggle('is-active', active);
    if (active) link.setAttribute('aria-current', 'location');
    else link.removeAttribute('aria-current');
  });
}
let navigationPending = false;
window.addEventListener('scroll', () => {
  if (navigationPending) return;
  navigationPending = true;
  requestAnimationFrame(() => { updateNavigation(); navigationPending = false; });
}, { passive: true });
window.addEventListener('resize', updateNavigation);
updateNavigation();

function renderLevyResult() {
  const levy = PREDICTIONS.find(p => p.label === 'Petroleum levy');
  if ($('levyValue')) {
    $('levyValue').textContent = Number.isFinite(levy?.forecast) ? number(levy.forecast) : '—';
  }
  if ($('levyRecommendationNote')) {
    $('levyRecommendationNote').textContent = Number.isFinite(levy?.forecast)
      ? 'Regression-tree recommendation for ' + levy.targetDate + '.'
      : 'Recommendation pending.';
  }
  document.querySelectorAll('.levy-band-scale span').forEach(el => {
    const active = Number.isFinite(levy?.forecast) && el.dataset.band === levy.band;
    el.classList.toggle('is-on', active);
    if (active) el.setAttribute('aria-current', 'true'); else el.removeAttribute('aria-current');
  });
}

function renderLevySummary() {
  const price = PREDICTIONS.find(p => p.label === 'Petrol price');
  const levy = PREDICTIONS.find(p => p.label === 'Petroleum levy');
  if (!$('buildup') || !$('buildupKeys')) return;

  $('buildup').replaceChildren(); $('buildupKeys').replaceChildren();
  if (!price || !levy || price.date !== levy.date || !(price.current > 0) || !(levy.current >= 0 && levy.current <= price.current)) {
    if ($('buildupNote')) $('buildupNote').textContent = 'Matching dated price components are unavailable.';
    return;
  }
  const segments = [['Other costs and taxes', price.current - levy.current], ['Petroleum levy', levy.current]];
  segments.forEach(([label, value]) => {
    const span = document.createElement('span');
    span.style.width = (value / price.current * 100) + '%';
    span.title = label + ': PKR ' + number(value);
    $('buildup').append(span);

    const row = document.createElement('div'), dt = document.createElement('dt'), dd = document.createElement('dd');
    dt.textContent = label; dd.textContent = 'PKR ' + number(value);
    row.append(dt, dd); $('buildupKeys').append(row);
  });
  $('buildup').setAttribute('aria-label', segments.map(([label, value]) => label + ': PKR ' + number(value)).join('; '));
  if ($('buildupNote')) $('buildupNote').textContent = 'Total: PKR ' + number(price.current) + ' / litre · ' + price.date + '.';
}

// DEBUG: render the reference column immediately, even when a CSV is missing.
PREDICTIONS = [
  { label: 'Brent crude', unit: 'USD / bbl', current: null, forecast: null },
  { label: 'Petrol price', unit: 'PKR / litre', current: null, forecast: null },
  { label: 'Petroleum levy', unit: 'PKR / litre', current: null, forecast: null, band: null }
];
renderCards();

// DEBUG: index.html is in frontend; ../data/ accesses the sibling data folder.
// DEBUG: each dataset has its own cache. Failed requests can be retried on the next click.
const dataTasks = new Map();
function dataset(key, work) {
  if (!dataTasks.has(key)) {
    dataTasks.set(key, work().catch(error => {
      dataTasks.delete(key);
      throw error;
    }));
  }
  return dataTasks.get(key);
}
function ensureBrent() {
  return dataset('brent', async () => {
    const rows = await loadCSV(
      ['../data/processed/brent_daily.csv', '../data/raw/brent_daily.csv'], ['price', 'close']);
    rows.sort((a, b) => String(a.date || '').localeCompare(String(b.date || '')));
    const valid = rows.map(row => ({ row, value: Number(row.price ?? row.close) }))
      .filter(item => Number.isFinite(item.value) && item.value > 0);
    if (!valid.length) throw new Error('Brent CSV contains no positive prices.');
    BRENT_PRICES = valid.map(item => item.value);
    const last = valid.at(-1);
    Object.assign(PREDICTIONS[0], { current: last.value, date: last.row.date });
    renderCards();
  });
}
function ensureBuildup() {
  return dataset('buildup', async () => {
    const rows = await loadCSV(
      ['../data/raw/petrol_price_buildup_fortnightly.csv', '../data/petrol_price_buildup_fortnightly.csv'],
      ['retail_petrol_price_model']);
    rows.sort((a, b) => String(a.period_end || '').localeCompare(String(b.period_end || '')));
    const last = rows.at(-1);
    const price = parseFloat(last.retail_petrol_price_model);
    const levy = parseFloat(last.petroleum_levy_pkr_l) + (parseFloat(last.carbon_levy_pkr_l) || 0);
    Object.assign(PREDICTIONS[1], { current: Number.isFinite(price) ? price : null, date: last.period_end });
    Object.assign(PREDICTIONS[2], { current: Number.isFinite(levy) ? levy : null, date: last.period_end });
    renderCards(); renderLevySummary();
  });
}
function ensureUsd() {
  return dataset('usd', async () => {
    const rows = await loadCSV(['../data/raw/usd_pkr_daily.csv'], ['usd_pkr']);
    rows.sort((a, b) => String(a.date || '').localeCompare(String(b.date || '')));
    USD_PRICES = rows.map(row => Number(row.usd_pkr)).filter(value => Number.isFinite(value) && value > 0);
    if (!USD_PRICES.length) throw new Error('USD / PKR CSV contains no positive rates.');
  });
}
function ensureTraining() {
  // DEBUG: only non-Brent tracker inputs need this frontend dataset.
  // A filename with a space is supported as well as levy_training.csv.
  return dataset('training', async () => {
    HISTORY = await loadCSV([
      '../data/processed/levy_training.csv', '../data/levy_training.csv',
      '../data/processed/levy training.csv', '../data/levy training.csv'
    ], ['usd_pkr', 'retail_petrol_price']);
    HISTORY.sort((a, b) => String(a.period_start || '').localeCompare(String(b.period_start || '')));
  });
}

// Scroll fade-in
if (!matchMedia('(prefers-reduced-motion: reduce)').matches && 'IntersectionObserver' in window) {
const io = new IntersectionObserver(entries => entries.forEach(e => {
if (e.intersectionRatio >= .12) e.target.classList.add('is-visible');
else if (!e.isIntersecting) e.target.classList.remove('is-visible');
}), { threshold: [0, .12] });
const watchReveal = () => document.querySelectorAll('.hero-banner,.hero-modules .module-card,.section-head,.panel,.card').forEach(el => {
el.classList.add('reveal');
el.style.transitionDelay = Math.min([...el.parentElement.children].indexOf(el), 4) * 80 + 'ms';
io.observe(el);
});
watchReveal();
if ($('cards')) {
new MutationObserver(watchReveal).observe($('cards'), { childList: true });
}
}
// Cursor-following glow
const GLOW_SEL = '.btn,.module-card,.nav a,.theme-toggle';
const moveGlow = e => {
const el = e.target.closest && e.target.closest(GLOW_SEL);
if (!el) return;
const r = el.getBoundingClientRect();
el.style.setProperty('--mx', e.clientX - r.left + 'px');
el.style.setProperty('--my', e.clientY - r.top + 'px');
};
document.addEventListener('pointermove', moveGlow, { passive: true });
document.addEventListener('pointerover', moveGlow, { passive: true });
function message(id, text) { if ($(id)) $(id).textContent = text; }
function show(id, visible) { if ($(id)) $(id).hidden = !visible; }
function reportError(id, error) {
  console.error('[DEBUG]', error);
  message(id, 'Error: ' + (error.message || String(error)));
}
// DEBUG: reuse the existing wrapper contract. Do not invent C++ exports.
async function backend(methods) {
  const api = typeof LevyWasm !== 'undefined' ? LevyWasm : window.LevyWasm;
  if (!api) throw new Error('LevyWasm is missing. Load levy_wasm.js and levy-wasm.js before app.js.');
  if (api.ready && typeof api.ready.then === 'function') await api.ready;
  for (const method of methods) {
    if (typeof api[method] !== 'function') throw new Error('LevyWasm.' + method + '() is missing from levy-wasm.js.');
  }
  return api;
}
function resultObject(result) {
  return typeof result === 'string' ? JSON.parse(result) : result;
}
let busy = false;
let lastForecast = null;
function setBusy(value, panelId) {
  busy = value;
  // DEBUG: serialize WASM calls because the same module may have shared C++ state.
  for (const id of ['runForecast', 'runLevy', 'forecastInput', 'forecastHorizon', 'window']) if ($(id)) $(id).disabled = value;
  if ($(panelId)) $(panelId).setAttribute('aria-busy', String(value));
}
const paint = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
function progress(id, text) {
  const list = $(id); if (!list) return;
  const li = document.createElement('li'); li.dataset.state = 'running'; li.textContent = text;
  list.replaceChildren(li);
}
function selectedSeries() {
  const key = $('forecastInput')?.value || 'forecast_crude_price';
  if (key === 'forecast_crude_price') return { values: BRENT_PRICES, label: 'Brent crude', unit: 'USD / bbl', period: 'daily observation' };
  const labels = { usd_pkr: ['USD / PKR', 'PKR / USD'], retail_petrol_price: ['Retail petrol', 'PKR / litre'] };
  if (!labels[key]) throw new Error('Unsupported input series: ' + key);
  const values = key === 'usd_pkr' ? USD_PRICES : HISTORY.map(row => Number(row[key])).filter(v => Number.isFinite(v) && v > 0);
  if (!values.length) throw new Error('No positive numeric values for ' + key + ' in levy_training.csv.');
  return { values, label: labels[key][0], unit: labels[key][1], period: key === 'usd_pkr' ? 'daily observation' : 'training dataset observation' };
}
// DEBUG: arimaMean returns ONE horizon mean, not a forecast trajectory.
// Plot history plus a labelled mean marker. Never fabricate future points or accuracy.
function drawForecast(series, horizon, mean) {
  const svg = $('forecastSvg'); if (!svg) return;
  const windowSize = Number($('window')?.value || 0);
  const history = windowSize > 0 ? series.values.slice(-windowSize) : series.values;
  const all = [...history, mean], min = Math.min(...all), max = Math.max(...all);
  const padding = (max - min) * .15 || Math.abs(max) * .05 || 1;
  const low = min - padding, high = max + padding;
  const x = i => 80 + i / Math.max(1, history.length - 1 + horizon) * 850;
  const y = v => 440 - (v - low) / (high - low) * 380;
  const ns = 'http://www.w3.org/2000/svg';
  const add = (tag, attrs, text) => {
    const el = document.createElementNS(ns, tag);
    for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, String(v));
    if (text !== undefined) el.textContent = text;
    svg.append(el); return el;
  };
  svg.replaceChildren();
  svg.setAttribute('aria-label', series.label + ': historical values and forecast horizon mean, ' + number(mean) + ' ' + series.unit);
  for (let i = 0; i <= 4; i++) {
    const v = low + (high - low) * i / 4;
    add('line', { x1: 80, x2: 930, y1: y(v), y2: y(v), stroke: 'currentColor', opacity: .15 });
    add('text', { x: 70, y: y(v) + 5, 'text-anchor': 'end', fill: 'currentColor', 'font-size': 16 }, number(v));
  }
  add('polyline', { points: history.map((v, i) => x(i) + ',' + y(v)).join(' '), fill: 'none', stroke: '#739aee', 'stroke-width': 3 });
  const end = x(history.length - 1 + horizon);
  add('circle', { cx: end, cy: y(mean), r: 7, fill: '#dcbf67' });
  add('text', { x: end, y: y(mean) - 18, 'text-anchor': 'end', fill: 'currentColor', 'font-size': 18 }, 'Horizon mean: ' + number(mean));
  add('text', { x: 80, y: 490, fill: 'currentColor', 'font-size': 17 }, 'History');
  add('text', { x: 930, y: 490, 'text-anchor': 'end', fill: 'currentColor', 'font-size': 17 }, 'Next ' + horizon + ' observations (mean)');
}
async function runForecast() {
  if (busy) return;
  lastForecast = null;
  setBusy(true, 'forecastPanel');
  show('chartProgress', true); show('forecastPlot', false); show('forecastLegend', false); show('chartIdle', false);
  show('modelInfo', false);
  message('runSummary', 'Loading inputs…');
  for (const id of ['statMape', 'statRmse', 'statMae', 'statOrder', 'statDiff', 'statNext']) message(id, '—');
  try {
    // DEBUG: Brent never waits for or requests levy_training.csv.
    const key = $('forecastInput')?.value || 'forecast_crude_price';
    if (key === 'forecast_crude_price') await ensureBrent();
    else if (key === 'usd_pkr') await ensureUsd();
    else await ensureTraining();
    const series = selectedSeries();
    const horizon = Number($('forecastHorizon')?.value || 12);
    if (!Number.isInteger(horizon) || horizon < 1 || horizon > 365) throw new Error('Select a forecast window from 1 to 365.');
    const api = await backend(['arimaMean']);
    progress('forecastSteps', 'Calculating C++ forecast…');
    message('runSummary', 'Calculating ' + series.label + '…');
    await paint();
    const mean = Number(await api.arimaMean(series.values, horizon));
    if (!Number.isFinite(mean) || mean <= 0) throw new Error('C++ returned no valid forecast. Check the model’s minimum training length and the input data.');
    lastForecast = { series, horizon, mean };
    drawForecast(series, horizon, mean);
    show('forecastPlot', true);
    message('forecastUnit', series.unit);
    message('runSummary', series.label + ': mean of the next ' + horizon + ' observations = ' + number(mean) + ' ' + series.unit + '. One observation = one ' + series.period + '.');
    message('accuracyNote', 'The arimaMean bridge does not return held-out accuracy or the selected model order.');
  } catch (error) { reportError('runSummary', error); }
  finally { show('chartProgress', false); setBusy(false, 'forecastPanel'); }
}
async function runLevy() {
  if (busy) return;
  setBusy(true, 'levyPanel'); show('levyProgress', true); show('levyResult', true);
  message('levyRecommendationNote', 'Loading inputs…');
  message('levyValue', '—');
  document.querySelectorAll('.levy-band-scale span').forEach(el => { el.classList.remove('is-on'); el.removeAttribute('aria-current'); });
  for (const id of ['treeMape', 'treeRmse', 'treeMae']) message(id, '—');
  try {
    // DEBUG: live() owns C++ training-data loading; fetching a browser CSV does not mount it in WASM.
    await ensureBrent();
    const levy = PREDICTIONS.find(p => p.label === 'Petroleum levy');
    if (levy) { levy.forecast = null; levy.band = null; renderCards(); }
    if (!BRENT_PRICES.length) throw new Error('Brent price history is empty.');
    const api = await backend(['arimaMean', 'live']);
    progress('levySteps', 'Calculating crude forecast…'); await paint();
    const crude = Number(await api.arimaMean(BRENT_PRICES, 10));
    // DEBUG: no made-up fallback of $75 when C++ fails.
    if (!Number.isFinite(crude) || crude <= 0) throw new Error('C++ crude forecast failed. Check training data length.');
    progress('levySteps', 'Calculating levy recommendation…'); await paint();
    const live = resultObject(await api.live(crude));
    if (!live || !live.ok) throw new Error(live?.error || 'C++ levy calculation failed. Check WASM CSV file mounting.');
    if (live.levy == null || live.levy === '' || !Number.isFinite(Number(live.levy)) || Number(live.levy) < 0) throw new Error('C++ returned an invalid levy.');
    levy.forecast = Number(live.levy);
    levy.band = String(live.band || '').trim().toLowerCase().replace(/[_\s]+/g, '-');
    levy.targetDate = live.periodStart || 'the next period';
    const crudeCard = PREDICTIONS.find(p => p.label === 'Brent crude');
    if (crudeCard) { crudeCard.forecast = crude; crudeCard.targetDate = 'Next 10 daily observations (mean)'; }
    renderLevyResult(); renderCards();
    // DEBUG: a report failure must not discard a successful recommendation.
    try {
      if (typeof api.report !== 'function') throw new Error('The WASM bridge does not expose report().');
      progress('levySteps', 'Reading model evaluation…'); await paint();
      const report = resultObject(await api.report());
      if (!report?.ok) throw new Error(report?.error || 'C++ evaluation failed.');
      const section = String(report.output || '').split('TEST PERFORMANCE')[1] || '';
      let count = 0;
      for (const [label, id] of [['MAPE', 'treeMape'], ['RMSE', 'treeRmse'], ['MAE', 'treeMae']]) {
        const match = section.match(new RegExp(label + ':\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)'));
        if (match && Number.isFinite(Number(match[1]))) { message(id, number(Number(match[1])) + (label === 'MAPE' ? '%' : '')); count++; }
      }
      message('treeNote', count === 3 ? 'Held-out regression-tree errors; lower is better.' : 'Evaluation output did not contain all three test metrics.');
    } catch (error) { reportError('treeNote', error); }
  } catch (error) { reportError('levyRecommendationNote', error); }
  finally { show('levyProgress', false); setBusy(false, 'levyPanel'); }
}
// DEBUG: attach the previously missing forecast handler and the recommendation handler.
$('runForecast')?.addEventListener('click', runForecast);
$('runLevy')?.addEventListener('click', runLevy);
for (const id of ['forecastInput', 'forecastHorizon']) {
  $(id)?.addEventListener('change', () => {
    lastForecast = null;
    show('forecastPlot', false); show('forecastLegend', false);
    message('runSummary', 'Settings changed. Run forecast to calculate the selected input and window.');
  });
}
$('window')?.addEventListener('change', () => {
  if (lastForecast) drawForecast(lastForecast.series, lastForecast.horizon, lastForecast.mean);
});
// DEBUG: opening HTML through file:// blocks browser CSV/WASM loading.
if (location.protocol === 'file:') {
  const error = new Error('Open this project through Live Server or http://localhost, not by double-clicking index.html.');
  reportError('runSummary', error); reportError('levyRecommendationNote', error);
} else {
  // DEBUG: load cards independently; a build-up failure cannot block Brent forecasting.
  ensureBrent().catch(error => console.error('[DEBUG] Brent history:', error));
  ensureBuildup().catch(error => reportError('buildupNote', error));
}

}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot, { once: true });
else boot();
})();
