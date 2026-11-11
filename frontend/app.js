const $ = id => document.getElementById(id);
const number = v => v == null ? '—' : new Intl.NumberFormat('en-PK', { maximumFractionDigits: 2 }).format(v);

let HISTORY = [];
let PREDICTIONS = [];
let BRENT_PRICES = [];

function parseCSV(text) {
  const lines = text.trim().split(/\r?\n/).filter(Boolean);
  if (lines.length < 2) return [];
  const firstLine = lines.at(0);
  const delim = firstLine.includes('\t') ? '\t' : ',';
  const headers = firstLine.split(delim).map(h => h.trim());
  return lines.slice(1).map(line => {
    const cols = line.split(delim);
    return Object.fromEntries(headers.map((h, i) => [h, cols.at(i)?.trim()]));
  });
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

async function initDataFromCSV() {
  try {
    const [brentRes, buildupRes, trainingRes] = await Promise.all([
      fetch('./data/brent_daily.csv'),
      fetch('./data/petrol_price_buildup_fortnightly.csv'),
      fetch('./data/levy_training.csv')
    ]);

    const brentRows = parseCSV(await brentRes.text());
    const buildupRows = parseCSV(await buildupRes.text());
    const trainingRows = parseCSV(await trainingRes.text());

    HISTORY = trainingRows;
    BRENT_PRICES = brentRows
      .map(r => parseFloat(r.price ?? r.close ?? Object.values(r).at(1)))
      .filter(v => Number.isFinite(v) && v > 0);

    const latestBrent = brentRows.at(-1) || {};
    const latestBuildup = buildupRows.at(-1) || {};

    const brentVal = parseFloat(latestBrent.price ?? latestBrent.close ?? Object.values(latestBrent).at(1));
    const brentDate = latestBrent.date ?? Object.values(latestBrent).at(0);
    const currentPrice = parseFloat(latestBuildup.retail_petrol_price_model);
    const currentLevy = parseFloat(latestBuildup.petroleum_levy_pkr_l) + parseFloat(latestBuildup.carbon_levy_pkr_l);

    PREDICTIONS = [
      {
        label: 'Brent crude',
        current: Number.isFinite(brentVal) ? brentVal : null,
        date: brentDate,
        unit: 'USD / bbl',
        forecast: null
      },
      {
        label: 'Petrol price',
        current: Number.isFinite(currentPrice) ? currentPrice : null,
        date: latestBuildup.period_end,
        unit: 'PKR / litre',
        forecast: null
      },
      {
        label: 'Petroleum levy',
        current: Number.isFinite(currentLevy) ? currentLevy : null,
        date: latestBuildup.period_end,
        targetDate: null,
        unit: 'PKR / litre',
        forecast: null,
        band: null
      }
    ];

    renderCards();
    renderLevySummary();
  } catch (err) {
    console.error('Failed to load CSV data:', err);
  }
}

const STEP_MS = 1000;
const sleep = ms => new Promise(r => setTimeout(r, ms));

async function runLevy() {
  const btn = $('runLevy'), panel = $('levyPanel'), steps = $('levySteps');
  if (!btn || !panel || !steps) return;

  btn.disabled = true; panel.setAttribute('aria-busy', 'true');
  if ($('levyResult')) $('levyResult').hidden = true;
  if ($('levyProgress')) $('levyProgress').hidden = false;
  steps.replaceChildren();

  const treeSteps = [
    'Load ' + HISTORY.length + ' training rows',
    'Search best split at each node',
    'Grow and prune the tree',
    'Route latest data to a leaf',
    'Read levy from leaf'
  ];

  const rows = treeSteps.map(t => {
    const li = document.createElement('li'), name = document.createElement('span'), st = document.createElement('span');
    li.dataset.state = 'waiting'; name.textContent = t; st.textContent = 'Waiting';
    li.append(name, st); steps.append(li); return [li, st];
  });

  const wasmTask = (async () => {
    if (typeof LevyWasm === 'undefined') {
      throw new Error('LevyWasm is not loaded. Check script tags in index.html.');
    }
    let crudeForecast = await LevyWasm.arimaMean(BRENT_PRICES, 10);
    if (!(crudeForecast > 0)) {
      crudeForecast = BRENT_PRICES.at(-1) || 75.0;
    }
    const liveRes = await LevyWasm.live(crudeForecast);
    const reportRes = await LevyWasm.report();
    return { crudeForecast, liveRes, reportRes };
  })();

  for (const [li, st] of rows) {
    li.dataset.state = 'running'; st.textContent = 'Running';
    await sleep(STEP_MS);
    li.dataset.state = 'done'; st.textContent = 'Done';
  }

  try {
    const { crudeForecast, liveRes, reportRes } = await wasmTask;

    const crudeCard = PREDICTIONS.find(p => p.label === 'Brent crude');
    if (crudeCard && Number.isFinite(crudeForecast)) {
      crudeCard.forecast = crudeForecast;
      crudeCard.targetDate = liveRes.periodStart || 'Next 10 days';
    }

    const levy = PREDICTIONS.find(p => p.label === 'Petroleum levy');
    if (liveRes.ok && levy) {
      levy.forecast = liveRes.levy;
      levy.band = liveRes.band;
      levy.targetDate = liveRes.periodStart;
    } else if (!liveRes.ok) {
console.error('WASM Levy Error:', liveRes.error);
}
if (reportRes.ok) {
const testSection = reportRes.output.split('TEST PERFORMANCE').at(1) || '';
const mape = (testSection.match(/MAPE:\s+([\d.]+)/) || []).at(1);
const rmse = (testSection.match(/RMSE:\s+([\d.]+)/) || []).at(1);
const mae = (testSection.match(/MAE:\s+([\d.]+)/) || []).at(1);
if ($('treeMape') && mape) $('treeMape').textContent = mape + '%';
if ($('treeRmse') && rmse) $('treeRmse').textContent = rmse;
if ($('treeMae') && mae) $('treeMae').textContent = mae;
}
} catch (e) {
console.error('WASM Execution Error:', e);
}
await sleep(400);
if ($('levyProgress')) $('levyProgress').hidden = true;
if ($('levyResult')) $('levyResult').hidden = false;
renderLevyResult();
renderCards();
panel.setAttribute('aria-busy', 'false'); btn.disabled = false;
}
if ($('runLevy')) {
$('runLevy').addEventListener('click', runLevy);
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
// Boot from CSVs
initDataFromCSV();
// END OF APP.JS