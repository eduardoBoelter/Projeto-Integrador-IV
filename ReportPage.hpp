#pragma once

// ---------------------------------------------------------------------------
// Etapa 10: modelo da tela de resultados (resultados.html).
//
// Página HTML completa, com CSS e JavaScript, sem nenhuma dependência externa:
// abre em qualquer navegador, mesmo sem internet. O simulador grava
// REPORT_PAGE_BEFORE, depois os dados em JSON e depois REPORT_PAGE_AFTER
// (veja Report.hpp). Os gráficos são desenhados em SVG pelo navegador.
//
// O texto está dividido em pedaços menores que 12 KB porque alguns
// compiladores limitam o tamanho de uma string literal.
// ---------------------------------------------------------------------------

inline const char* const REPORT_PAGE_BEFORE[] = {
R"PI4(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Resultados do wear leveling</title>
<style>
:root {
  color-scheme: light;
  --page: #f9f9f7; --surface: #fcfcfb; --ink: #0b0b0b; --ink-2: #52514e; --muted: #898781;
  --grid: #e1e0d9; --axis: #c3c2b7; --border: rgba(11, 11, 11, .10);
  --s1: #2a78d6; --s2: #eb6834; --track: #cde2fb; --critical: #d03b3b;
}
@media (prefers-color-scheme: dark) {
  :root {
    color-scheme: dark;
    --page: #0d0d0d; --surface: #1a1a19; --ink: #f0efec; --ink-2: #c3c2b7; --muted: #898781;
    --grid: #2c2c2a; --axis: #383835; --border: rgba(255, 255, 255, .10);
    --s1: #3987e5; --s2: #d95926; --track: #1d3350;
  }
}
* { box-sizing: border-box; }
html { -webkit-text-size-adjust: 100%; }
body { margin: 0; background: var(--page); color: var(--ink);
  font: 15px/1.5 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
main { max-width: 1120px; margin: 0 auto; padding: 32px 24px 64px; }
h1 { font-size: 28px; line-height: 1.2; margin: 0 0 6px; font-weight: 650; }
.sub { color: var(--ink-2); margin: 0; }
.meta { color: var(--muted); font-size: 13px; margin: 6px 0 0; }
.tabs { display: inline-flex; flex-wrap: wrap; gap: 4px; padding: 4px; margin: 24px 0 4px;
  border-radius: 10px; background: var(--surface); border: 1px solid var(--border); }
.tabs button { font: inherit; font-size: 14px; border: 0; background: transparent; color: var(--ink-2);
  padding: 8px 14px; border-radius: 7px; cursor: pointer; }
.tabs button[aria-selected="true"] { background: var(--ink); color: var(--surface); }
.desc { color: var(--ink-2); max-width: 860px; margin: 12px 0 0; }
section { margin-top: 32px; }
h2 { font-size: 18px; margin: 0 0 4px; font-weight: 620; }
.note { color: var(--muted); font-size: 13px; margin: 0 0 12px; max-width: 860px; }
.card { background: var(--surface); border: 1px solid var(--border); border-radius: 12px; padding: 20px; }
.hero { display: grid; grid-template-columns: minmax(240px, 1fr) 2fr; gap: 16px; }
.hero-num { font-size: 64px; line-height: 1; font-weight: 650; letter-spacing: -.02em; }
.hero-txt { color: var(--ink-2); margin-top: 12px; }
.hero-sub { color: var(--muted); font-size: 13px; margin-top: 8px; }
.tiles { display: grid; grid-template-columns: repeat(2, 1fr); gap: 16px; }
.tile { padding: 16px 20px; }
.tile .lab { color: var(--ink-2); font-size: 13px; }
.tile .val { font-size: 28px; font-weight: 620; line-height: 1.25; margin-top: 2px; }
.tile .cmp { color: var(--muted); font-size: 13px; }
.grid2 { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }
.grid4 { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 16px; }
.panel { padding: 16px; }
.panel h3 { font-size: 14px; margin: 0; font-weight: 600; }
.panel .stats { color: var(--muted); font-size: 12px; margin: 2px 0 10px; }
.grid4 .panel { display: flex; flex-direction: column; }
.grid4 .panel svg { margin-top: auto; }
svg { display: block; overflow: visible; }
svg text { font-family: inherit; fill: var(--muted); font-size: 11px; }
svg text.lbl { fill: var(--ink-2); font-size: 13px; }
svg text.val { fill: var(--ink); font-size: 12px; font-variant-numeric: tabular-nums; }
.legend { display: flex; flex-wrap: wrap; gap: 6px 18px; font-size: 13px; color: var(--ink-2); margin: 0 0 12px; }
.legend i { display: inline-block; width: 16px; height: 3px; border-radius: 2px; margin-right: 6px; vertical-align: 4px; }
.scale { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 10px; font-size: 12px; color: var(--muted); margin: 0 0 12px; }
.scale .ramp { width: 160px; height: 10px; border-radius: 5px; }
.bad { display: inline-flex; width: 14px; height: 14px; border-radius: 3px; background: var(--critical); color: #fff;
  font-size: 10px; font-weight: 700; align-items: center; justify-content: center; margin-left: 10px; }
.meter + .meter { margin-top: 14px; }
.meter .row { display: flex; justify-content: space-between; gap: 12px; font-size: 14px; }
.meter .row span:last-child { font-variant-numeric: tabular-nums; }
.meter .track { height: 8px; border-radius: 4px; background: var(--track); margin-top: 6px; }
.meter .fill { height: 100%; border-radius: 4px; background: var(--s1); }
.tablewrap { overflow-x: auto; padding: 8px 12px; }
table { border-collapse: collapse; width: 100%; font-size: 14px; }
th, td { padding: 8px 10px; text-align: right; border-bottom: 1px solid var(--grid); }
td { white-space: nowrap; font-variant-numeric: tabular-nums; }
th:first-child, td:first-child { text-align: left; white-space: normal; min-width: 150px; }
thead th { color: var(--ink-2); font-weight: 600; font-size: 13px; vertical-align: bottom; min-width: 96px; }
tbody tr:last-child td { border-bottom: 0; }
dl { margin: 0; display: grid; grid-template-columns: minmax(150px, max-content) 1fr; gap: 8px 20px; font-size: 14px; }
dt { font-weight: 600; }
dd { margin: 0; color: var(--ink-2); }
#tip { position: fixed; left: 0; top: 0; pointer-events: none; background: var(--ink); color: var(--surface);
  font-size: 12px; line-height: 1.45; padding: 6px 9px; border-radius: 6px; opacity: 0; max-width: 260px;
  white-space: pre-line; z-index: 10; transition: opacity .08s; }
footer { margin-top: 40px; color: var(--muted); font-size: 12px; }
@media (max-width: 860px) {
  .grid4 { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  .hero { grid-template-columns: 1fr; }
}
@media (max-width: 560px) {
  main { padding: 20px 16px 48px; }
  h1 { font-size: 23px; }
  .grid2 { grid-template-columns: minmax(0, 1fr); }
  .hero-num { font-size: 52px; }
  .tile .val { font-size: 22px; }
  dl { grid-template-columns: 1fr; gap: 2px; }
  dd { margin-bottom: 8px; }
}
</style>
</head>
<body>
<main>
  <header>
    <h1>Wear leveling em um pendrive NAND simulado</h1>
    <p class="sub">Resultados do simulador do Projeto Integrador IV</p>
    <p class="meta" id="meta"></p>
  </header>
  <div class="tabs" role="tablist" id="abas"></div>
  <p class="desc" id="descricao"></p>
  <div id="conteudo"></div>
  <section>
    <h2>Como ler esta página</h2>
    <div class="card"><dl id="glossario"></dl></div>
  </section>
  <footer id="rodape"></footer>
</main>
<div id="tip" role="tooltip"></div>
<script>
const DADOS = )PI4"
};

inline const char* const REPORT_PAGE_AFTER[] = {
R"PI4(;
</script>
<script>
'use strict';

const NOMES = {
  direto: 'Mapeamento direto',
  mapeamento: 'Mapeamento sem wear leveling',
  dinamico: 'Wear leveling dinâmico',
  estatico: 'Wear leveling dinâmico + estático'
};
const ABAS = { fs: 'Uso de pendrive (sistema de arquivos)', quente: 'Dados quentes (metadados)' };
const DESCRICOES = {
  fs: 'Fotos e músicas ocupam 60% do disco e nunca mudam (dados frios). Depois, documentos pequenos são ' +
      'editados, criados e apagados o tempo todo, e cada operação regrava os metadados do sistema de arquivos ' +
      '(dados quentes). A mesma sequência de operações é aplicada aos quatro modos da FTL até o primeiro bloco ' +
      'chegar ao limite de ciclos.',
  quente: 'O disco é preenchido uma vez. Depois, 90% das escritas vão para os LBAs 0 a 3 (metadados) e 10% para ' +
      'LBAs aleatórios, até o primeiro bloco chegar ao limite de ciclos. A mesma sequência é aplicada aos quatro modos.'
};
const GLOSSARIO = [
  ['Mapeamento direto', 'O LBA n fica sempre na mesma página física. Regravar uma página obriga a apagar o bloco ' +
    'inteiro, então os blocos dos metadados se desgastam muito rápido.'],
  ['Mapeamento sem wear leveling', 'FTL com tabela LBA → PBA e garbage collection: cada escrita vai para uma página ' +
    'livre, mas o bloco livre é escolhido sem olhar o desgaste.'],
  ['Wear leveling dinâmico', 'Ao abrir um bloco para escrita, a FTL escolhe o bloco livre com menos ciclos.'],
  ['Wear leveling estático', 'Além do dinâmico, a cada 64 escritas a FTL procura dados frios parados em blocos pouco ' +
    'gastos (diferença maior que 50 ciclos) e os copia para um bloco gasto, liberando o bloco novo.'],
  ['Ciclo P/E', 'Uma programação seguida de um apagamento do bloco. Cada bloco aguenta 1.000 ciclos; ao chegar ' +
    'nesse limite, vira bad block.'],
  ['Desvio padrão e CV', 'O quanto o desgaste varia entre os blocos. O coeficiente de variação (CV = desvio ÷ média) ' +
    'compara modos com médias diferentes: quanto menor, mais uniforme.'],
  ['Vida aproveitada', 'Média de ciclos dos blocos dividida pelo limite: a parte da capacidade total de apagamentos ' +
    'usada até a primeira falha.'],
  ['Amplificação de escrita', 'Páginas gravadas na NAND para cada escrita lógica. Mede o trabalho extra do garbage ' +
    'collection e das migrações; 1,00 significa nenhum trabalho extra.']
];
const LINHAS = [
  ['Escritas lógicas até a 1ª falha (fase de uso)', m => nf(m.escritas_carga)],
  ['Escrita total em que o 1º bloco falhou', m => m.falhou ? nf(m.escrita_falha) + ' (bloco ' + m.bloco_falha + ')' : 'sem falha'],
  ['Operações do sistema de arquivos', m => nf(m.operacoes), 'fs'],
  ['Média de ciclos P/E', m => nf(m.media, 1)],
  ['Desvio padrão', m => nf(m.desvio, 1)],
  ['Coeficiente de variação', m => nf(m.cv, 2)],
  ['Mínimo', m => nf(m.min)],
  ['Mediana', m => nf(m.mediana, m.mediana % 1 ? 1 : 0)],
  ['Máximo', m => nf(m.max)],
  ['Percentis 10 e 90', m => nf(m.p10) + ' e ' + nf(m.p90)],
  ['Blocos nunca apagados', m => nf(m.nunca)],
  ['Vida aproveitada', m => pct(m.vida)],
  ['Amplificação de escrita', m => nf(m.wa, 2)],
  ['Execuções do garbage collection', m => nf(m.gc)],
  ['Páginas copiadas pelo GC', m => nf(m.gc_paginas)],
  ['Migrações do WL estático', m => nf(m.migracoes)],
  ['Páginas movidas pelo WL estático', m => nf(m.wl_paginas)],
  ['Tempo de simulação', m => nf(m.segundos, 1) + ' s']
];

const RAMPA_CLARA = ['#eef4fc', '#cde2fb', '#9ec5f4', '#6da7ec', '#3987e5', '#256abf', '#184f95', '#0d366b'];
const RAMPA_ESCURA = ['#1e2a38', '#123a6b', '#184f95', '#256abf', '#3987e5', '#6da7ec', '#9ec5f4', '#cde2fb'];
const escuro = window.matchMedia('(prefers-color-scheme: dark)');
const NS = 'http://www.w3.org/2000/svg';
let atual = 0;

// ------------------------------------------------------------ utilidades
function nf(v, casas) {
  casas = casas || 0;
  return Number(v).toLocaleString('pt-BR', { minimumFractionDigits: casas, maximumFractionDigits: casas });
}
function compacto(v) {
  return new Intl.NumberFormat('pt-BR', { notation: 'compact', maximumFractionDigits: 1 }).format(v);
}
function pct(v) { return nf(100 * v, 1) + '%'; }
function svg(tag, attrs, pai, texto) {
  const e = document.createElementNS(NS, tag);
  for (const k in attrs) e.setAttribute(k, attrs[k]);
  if (texto !== undefined) e.textContent = texto;
  if (pai) pai.appendChild(e);
  return e;
}
function html(tag, classe, pai, texto) {
  const e = document.createElement(tag);
  if (classe) e.className = classe;
  if (texto !== undefined) e.textContent = texto;
  if (pai) pai.appendChild(e);
  return e;
}
function modo(cen, chave) { return cen.modos.find(m => m.chave === chave); }
function nome(m) { return NOMES[m.chave] || m.nome; }
function larguraUtil(e) {
  const s = getComputedStyle(e);
  return Math.floor(e.clientWidth - parseFloat(s.paddingLeft) - parseFloat(s.paddingRight));
}
function passoBonito(bruto) {
  if (!(bruto > 0)) return 1;
  const p = Math.pow(10, Math.floor(Math.log10(bruto)));
  const f = bruto / p;
  return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * p;
}
function escala(max, n) {
  const passo = passoBonito(max / n);
  const topo = Math.max(passo, Math.ceil(max / passo) * passo);
  const marcas = [];
  for (let v = 0; v <= topo + passo / 1000; v += passo) marcas.push(v);
  return { topo, marcas };
}
function corDesgaste(v, max) {
  const r = escuro.matches ? RAMPA_ESCURA : RAMPA_CLARA;
  const t = Math.max(0, Math.min(1, v / max)) * (r.length - 1);
  const i = Math.min(Math.floor(t), r.length - 2), f = t - i;
  const a = [1, 3, 5].map(k => parseInt(r[i].slice(k, k + 2), 16));
  const b = [1, 3, 5].map(k => parseInt(r[i + 1].slice(k, k + 2), 16));
  return 'rgb(' + a.map((x, k) => Math.round(x + (b[k] - x) * f)).join(',') + ')';
}
// Barra horizontal com a ponta arredondada e a base reta
function barraH(x, y, w, h) {
  const r = Math.min(4, w / 2, h / 2);
  return `M${x},${y}H${x + w - r}Q${x + w},${y} ${x + w},${y + r}V${y + h - r}Q${x + w},${y + h} ${x + w - r},${y + h}H${x}Z`;
}
// Coluna com o topo arredondado e a base reta
function barraV(x, base, w, h) {
  const r = Math.min(4, w / 2, h / 2), y = base - h;
  return `M${x},${base}V${y + r}Q${x},${y} ${x + r},${y}H${x + w - r}Q${x + w},${y} ${x + w},${y + r}V${base}Z`;
}

// ------------------------------------------------------------ dica (tooltip)
const tip = document.getElementById('tip');
function mostrarDica(texto, x, y) {
  tip.textContent = texto;
  tip.style.opacity = 1;
  const r = tip.getBoundingClientRect();
  let left = x + 14, top = y + 14;
  if (left + r.width > window.innerWidth - 8) left = x - r.width - 14;
  if (top + r.height > window.innerHeight - 8) top = y - r.height - 14;
  tip.style.transform = `translate(${Math.max(8, left)}px, ${Math.max(8, top)}px)`;
}
function esconderDica() { tip.style.opacity = 0; }
document.addEventListener('pointermove', e => {
  const alvo = e.target.closest ? e.target.closest('[data-tip]') : null;
  if (alvo) mostrarDica(alvo.getAttribute('data-tip'), e.clientX, e.clientY);
  else if (!(e.target.closest && e.target.closest('.cruz'))) esconderDica();
});
document.addEventListener('pointerleave', esconderDica);

function secao(pai, titulo, nota) {
  const s = html('section', '', pai);
  html('h2', '', s, titulo);
  if (nota) html('p', 'note', s, nota);
  return s;
}

// ------------------------------------------------------------ destaque
function destaque(pai, cen) {
  const est = modo(cen, 'estatico'), sem = modo(cen, 'mapeamento'), dir = modo(cen, 'direto');
  if (!est || !sem || !sem.escritas_carga) return;
  const s = html('section', 'hero', pai);
  const a = html('div', 'card', s);
  html('div', 'hero-num', a, nf(est.escritas_carga / sem.escritas_carga, 1) + '×');
  html('div', 'hero-txt', a, 'mais escritas até o primeiro bloco falhar com wear leveling dinâmico + estático ' +
    'do que sem wear leveling.');
  if (dir && dir.escritas_carga)
    html('div', 'hero-sub', a, 'Em relação ao mapeamento direto: ' + nf(est.escritas_carga / dir.escritas_carga) + '× mais.');
  const t = html('div', 'tiles', s);
  const bloco = (rotulo, valor, comparacao) => {
    const c = html('div', 'card tile', t);
    html('div', 'lab', c, rotulo);
    html('div', 'val', c, valor);
    html('div', 'cmp', c, comparacao);
  };
  bloco('Escritas até a 1ª falha', compacto(est.escritas_carga), 'sem wear leveling: ' + compacto(sem.escritas_carga));
  bloco('Vida aproveitada dos blocos', pct(est.vida), 'sem wear leveling: ' + pct(sem.vida));
  bloco('Desvio padrão do desgaste', nf(est.desvio, 1) + ' ciclos', 'sem wear leveling: ' + nf(sem.desvio, 1) + ' ciclos');
  bloco('Custo (amplificação de escrita)', nf(est.wa, 2), 'sem wear leveling: ' + nf(sem.wa, 2));
}

// ------------------------------------------------------------ escritas até a falha
function graficoEscritas(pai, cen) {
  const s = secao(pai, 'Escritas até o primeiro bloco falhar',
    'Escritas lógicas da fase de uso até algum bloco chegar ao limite de ciclos. O valor em × compara com o ' +
    'mapeamento sem wear leveling.');
  const card = html('div', 'card', s);
  const W = larguraUtil(card), estreito = W < 560;
  const esq = estreito ? 0 : 230, dir = 120, bh = 22, linha = estreito ? 58 : 42, n = cen.modos.length;
  const base = n * linha, H = base + 24;
  const max = Math.max(1, ...cen.modos.map(m => m.escritas_carga));
  const { topo, marcas } = escala(max, estreito ? 3 : 5);
  const larg = W - esq - dir;
  const x = v => esq + larg * v / topo;
  const g = svg('svg', { width: W, height: H, viewBox: `0 0 ${W} ${H}`, role: 'img',
    'aria-label': 'Escritas até o primeiro bloco falhar, por modo da FTL' }, card);
  for (const t of marcas) {
    svg('line', { x1: x(t), x2: x(t), y1: 0, y2: base, style: 'stroke:var(--grid)' }, g);
    svg('text', { x: x(t), y: base + 17, 'text-anchor': t === 0 ? 'start' : 'middle' }, g, compacto(t));
  }
  const sem = modo(cen, 'mapeamento');
  cen.modos.forEach((m, i) => {
    const y = estreito ? i * linha + 24 : i * linha + (linha - bh) / 2;
    svg('text', { x: estreito ? 0 : esq - 12, y: estreito ? i * linha + 16 : y + bh / 2 + 5,
      'text-anchor': estreito ? 'start' : 'end', class: 'lbl' }, g, nome(m));
    const w = Math.max(2, larg * m.escritas_carga / topo);
    const grupo = svg('g', { 'data-tip': nome(m) + '\n' + nf(m.escritas_carga) + ' escritas até a 1ª falha' +
      (m.falhou ? '\nbloco ' + m.bloco_falha + ' falhou' : '\nsem falha até o limite do experimento') }, g);
    svg('rect', { x: esq, y: y - 8, width: larg, height: bh + 16, fill: 'transparent' }, grupo);
    svg('path', { d: barraH(esq, y, w, bh), style: 'fill:var(--s1)' }, grupo);
    let rotulo = nf(m.escritas_carga);
    if (sem && sem.escritas_carga && m !== sem && m.chave !== 'direto')
      rotulo += ' · ' + nf(m.escritas_carga / sem.escritas_carga, 1) + '×';
    svg('text', { x: esq + w + 8, y: y + bh / 2 + 4, class: 'val' }, g, rotulo);
  });
  svg('line', { x1: esq, x2: esq, y1: 0, y2: base, style: 'stroke:var(--axis)' }, g);
}

// ------------------------------------------------------------ vida aproveitada
function graficoVida(pai, cen) {
  const s = secao(pai, 'Vida aproveitada até a primeira falha',
    'Média de ciclos dos blocos dividida pelo limite: quanto da capacidade total de apagamentos do pendrive ' +
    'foi usada antes do primeiro bloco falhar. Sem wear leveling, o pendrive "morre" com quase todos os blocos novos.');
  const card = html('div', 'card', s);
  for (const m of cen.modos) {
    const d = html('div', 'meter', card);
    d.setAttribute('data-tip', nome(m) + ': média de ' + nf(m.media, 1) + ' ciclos por bloco');
    const r = html('div', 'row', d);
    html('span', '', r, nome(m));
    html('span', '', r, pct(m.vida));
    const f = html('div', 'fill', html('div', 'track', d));
)PI4",
R"PI4(    f.style.width = Math.max(1, Math.min(100, 100 * m.vida)) + '%';
  }
}

// ------------------------------------------------------------ mapa de desgaste
function graficoMapas(pai, cen) {
  const lim = DADOS.geometria.ciclos_max;
  const s = secao(pai, 'Mapa de desgaste dos ' + DADOS.geometria.blocos + ' blocos',
    'Cada quadrado é um bloco, 16 por linha. Quanto mais escuro, mais ciclos de apagamento. Passe o mouse ' +
    'sobre um bloco para ver o número dele e os ciclos.');
  const leg = html('div', 'scale', s);
  html('span', '', leg, '0 ciclos');
  const rampa = html('span', 'ramp', leg);
  rampa.style.background = 'linear-gradient(90deg,' + (escuro.matches ? RAMPA_ESCURA : RAMPA_CLARA).join(',') + ')';
  html('span', '', leg, nf(lim) + ' ciclos');
  html('span', 'bad', leg, '✕');
  html('span', '', leg, 'bad block (o bloco que falhou)');
  const grade = html('div', 'grid2', s);
  for (const m of cen.modos) {
    const p = html('div', 'card panel', grade);
    html('h3', '', p, nome(m));
    html('div', 'stats', p, 'média ' + nf(m.media, 1) + ' · desvio ' + nf(m.desvio, 1) + ' · mín ' + nf(m.min) +
      ' · máx ' + nf(m.max));
    const W = larguraUtil(p), cols = 16, linhas = Math.ceil(m.pe.length / cols), gap = 2;
    const lado = (W - gap * (cols - 1)) / cols, H = linhas * lado + gap * (linhas - 1);
    const g = svg('svg', { width: W, height: H, viewBox: `0 0 ${W} ${H}`, role: 'img',
      'aria-label': 'Mapa de desgaste: ' + nome(m) }, p);
    const ruins = m.ruins || [];
    m.pe.forEach((v, i) => {
      const x = (i % cols) * (lado + gap), y = Math.floor(i / cols) * (lado + gap);
      const ruim = ruins.includes(i);
      const grupo = svg('g', { 'data-tip': 'Bloco ' + i + ': ' + nf(v) + ' ciclos' + (ruim ? ' (bad block)' : '') }, g);
      svg('rect', { x, y, width: lado, height: lado, rx: 2,
        style: 'fill:' + (ruim ? 'var(--critical)' : corDesgaste(v, lim)) }, grupo);
      if (ruim) svg('text', { x: x + lado / 2, y: y + lado / 2 + 4, 'text-anchor': 'middle',
        style: 'fill:#fff;font-size:11px;font-weight:700' }, grupo, '✕');
    });
  }
}

// ------------------------------------------------------------ histogramas
function graficoHistogramas(pai, cen) {
  const lim = DADOS.geometria.ciclos_max, nb = 10, faixa = lim / nb;
  const s = secao(pai, 'Distribuição do desgaste',
    'Quantos blocos estão em cada faixa de ' + nf(faixa) + ' ciclos. Sem wear leveling, a maioria dos blocos ' +
    'fica quase nova enquanto poucos chegam ao limite; com o wear leveling estático, os blocos chegam perto do ' +
    'limite juntos.');
  const contagens = cen.modos.map(m => {
    const c = new Array(nb).fill(0);
    m.pe.forEach(v => c[Math.min(nb - 1, Math.floor(v / faixa))]++);
    return c;
  });
  const { topo, marcas } = escala(Math.max(1, ...contagens.flat()), 4);
  const grade = html('div', 'grid4', s);
  cen.modos.forEach((m, k) => {
    const p = html('div', 'card panel', grade);
    html('h3', '', p, nome(m));
    html('div', 'stats', p, 'coeficiente de variação ' + nf(m.cv, 2));
    const W = larguraUtil(p), H = 170, esq = 30, base = H - 22, topo0 = 6;
    const g = svg('svg', { width: W, height: H, viewBox: `0 0 ${W} ${H}`, role: 'img',
      'aria-label': 'Distribuição do desgaste: ' + nome(m) }, p);
    const y = v => base - (base - topo0) * v / topo;
    for (const t of marcas) {
      svg('line', { x1: esq, x2: W, y1: y(t), y2: y(t), style: 'stroke:var(--grid)' }, g);
      svg('text', { x: esq - 6, y: y(t) + 4, 'text-anchor': 'end' }, g, nf(t));
    }
    const slot = (W - esq) / nb, bw = Math.min(24, slot - 2);
    contagens[k].forEach((c, i) => {
      const fim = i === nb - 1 ? lim : (i + 1) * faixa - 1;
      const grupo = svg('g', { 'data-tip': nf(i * faixa) + ' a ' + nf(fim) + ' ciclos: ' + c + (c === 1 ? ' bloco' : ' blocos') }, g);
      svg('rect', { x: esq + i * slot, y: topo0, width: slot, height: base - topo0, fill: 'transparent' }, grupo);
      if (c > 0) svg('path', { d: barraV(esq + i * slot + (slot - bw) / 2, base, bw, Math.max(2, base - y(c))),
        style: 'fill:var(--s1)' }, grupo);
    });
    svg('line', { x1: esq, x2: W, y1: base, y2: base, style: 'stroke:var(--axis)' }, g);
    [[0, 'start'], [lim / 2, 'middle'], [lim, 'end']].forEach(([v, ancora]) =>
      svg('text', { x: esq + (W - esq) * v / lim, y: H - 4, 'text-anchor': ancora }, g, nf(v)));
  });
}

// ------------------------------------------------------------ evolução
function graficoEvolucao(pai, cen) {
  const lim = DADOS.geometria.ciclos_max;
  const s = secao(pai, 'Evolução do desgaste até a primeira falha',
    'Ciclos do bloco mais gasto e média dos blocos ao longo das escritas. Quando o máximo chega a ' + nf(lim) +
    ', o primeiro bloco falha. Quanto mais perto a média fica do máximo, mais uniforme é o desgaste. ' +
    'Cada gráfico tem a própria escala de escritas.');
  const leg = html('div', 'legend', s);
  const item = (cor, texto) => { const e = html('span', '', leg); html('i', '', e).style.background = cor; e.append(texto); };
  item('var(--s2)', 'Bloco mais gasto (máximo)');
  item('var(--s1)', 'Média dos blocos');
  const grade = html('div', 'grid2', s);
  for (const m of cen.modos) {
    const pts = m.evolucao;
    if (!pts || pts.length < 2) continue;
    const p = html('div', 'card panel', grade);
    html('h3', '', p, nome(m));
    html('div', 'stats', p, m.falhou ? 'primeiro bloco falhou na escrita ' + nf(m.escrita_falha)
      : 'sem falha até ' + nf(m.escrita_falha) + ' escritas');
    const W = larguraUtil(p), H = 210, esq = 38, dir = 78, topo0 = 8, base = H - 24;
    const x0 = pts[0][0], x1 = pts[pts.length - 1][0];
    const X = v => esq + (W - esq - dir) * (v - x0) / Math.max(1, x1 - x0);
    const Y = v => base - (base - topo0) * v / lim;
    const g = svg('svg', { width: W, height: H, viewBox: `0 0 ${W} ${H}`, role: 'img',
      'aria-label': 'Evolução do desgaste: ' + nome(m) }, p);
    for (let t = 0; t <= lim; t += lim / 4) {
      svg('line', { x1: esq, x2: W - dir, y1: Y(t), y2: Y(t), style: 'stroke:var(--grid)' }, g);
      svg('text', { x: esq - 6, y: Y(t) + 4, 'text-anchor': 'end' }, g, nf(t));
    }
    const passo = passoBonito((x1 - x0) / 3);
    for (let v = Math.ceil(x0 / passo) * passo; v <= x1; v += passo)
      svg('text', { x: X(v), y: base + 17, 'text-anchor': 'middle' }, g, compacto(v));
    svg('line', { x1: esq, x2: W - dir, y1: base, y2: base, style: 'stroke:var(--axis)' }, g);
    const caminho = k => pts.map((q, i) => (i ? 'L' : 'M') + X(q[0]).toFixed(1) + ',' + Y(q[k]).toFixed(1)).join('');
    const traco = 'fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round;stroke:';
    svg('path', { d: caminho(3), style: traco + 'var(--s2)' }, g);
    svg('path', { d: caminho(1), style: traco + 'var(--s1)' }, g);
    const u = pts[pts.length - 1], yMax = Y(u[3]), yMed = Y(u[1]);
    const anel = 'stroke:var(--surface);stroke-width:2;fill:';
    svg('circle', { cx: X(u[0]), cy: yMax, r: 4, style: anel + 'var(--s2)' }, g);
    svg('circle', { cx: X(u[0]), cy: yMed, r: 4, style: anel + 'var(--s1)' }, g);
    const afastados = yMed - yMax >= 15;
    svg('text', { x: X(u[0]) + 10, y: afastados ? yMax + 4 : Math.min(yMax, yMed) - 2, class: 'val' }, g, 'máx ' + nf(u[3]));
    svg('text', { x: X(u[0]) + 10, y: afastados ? yMed + 4 : Math.min(yMax, yMed) + 13, class: 'val' }, g, 'média ' + nf(u[1]));
    // Linha vertical que acompanha o mouse
    const cruz = svg('g', { style: 'display:none' }, g);
    const vl = svg('line', { y1: topo0, y2: base, style: 'stroke:var(--axis)' }, cruz);
    const c1 = svg('circle', { r: 4, style: anel + 'var(--s2)' }, cruz);
    const c2 = svg('circle', { r: 4, style: anel + 'var(--s1)' }, cruz);
    const area = svg('rect', { x: esq, y: topo0, width: W - esq - dir, height: base - topo0, fill: 'transparent', class: 'cruz' }, g);
    area.addEventListener('pointermove', e => {
      const r = g.getBoundingClientRect();
      const alvo = x0 + (e.clientX - r.left - esq) / (W - esq - dir) * (x1 - x0);
      let lo = 0, hi = pts.length - 1;
      while (lo < hi) { const mid = (lo + hi) >> 1; if (pts[mid][0] < alvo) lo = mid + 1; else hi = mid; }
      if (lo > 0 && Math.abs(pts[lo - 1][0] - alvo) < Math.abs(pts[lo][0] - alvo)) lo--;
      const q = pts[lo], cx = X(q[0]);
      cruz.style.display = '';
      vl.setAttribute('x1', cx); vl.setAttribute('x2', cx);
      c1.setAttribute('cx', cx); c1.setAttribute('cy', Y(q[3]));
      c2.setAttribute('cx', cx); c2.setAttribute('cy', Y(q[1]));
      mostrarDica(nf(q[0]) + ' escritas\nmáximo: ' + nf(q[3]) + ' ciclos\nmédia: ' + nf(q[1], 1) +
        ' · desvio: ' + nf(q[2], 1), e.clientX, e.clientY);
    });
    area.addEventListener('pointerleave', () => { cruz.style.display = 'none'; esconderDica(); });
  }
}

// ------------------------------------------------------------ tabela
function tabela(pai, cen) {
  const s = secao(pai, 'Tabela comparativa', 'Todos os números medidos em cada modo. Os mesmos dados estão nos ' +
    'arquivos CSV gerados pelo simulador.');
  const t = html('table', '', html('div', 'card tablewrap', s));
  const cab = html('tr', '', html('thead', '', t));
  html('th', '', cab, 'Métrica');
  cen.modos.forEach(m => html('th', '', cab, nome(m)));
  const corpo = html('tbody', '', t);
  for (const [rotulo, valor, so] of LINHAS) {
    if (so && so !== cen.chave) continue;
    const tr = html('tr', '', corpo);
    html('td', '', tr, rotulo);
    cen.modos.forEach(m => html('td', '', tr, valor(m)));
  }
}

// ------------------------------------------------------------ página
function desenhar() {
  const geo = DADOS.geometria;
  const mb = geo.blocos * geo.paginas_por_bloco * geo.tamanho_pagina / 1048576;
  document.getElementById('meta').textContent = 'Gerado em ' + DADOS.gerado + ' · ' + geo.blocos + ' blocos × ' +
    geo.paginas_por_bloco + ' páginas de ' + nf(geo.tamanho_pagina / 1024) + ' KB (' + nf(mb) + ' MB) · limite de ' +
    nf(geo.ciclos_max) + ' ciclos por bloco';
  const abas = document.getElementById('abas');
  abas.textContent = '';
  abas.style.display = DADOS.cenarios.length > 1 ? '' : 'none';
  DADOS.cenarios.forEach((c, i) => {
    const b = html('button', '', abas, ABAS[c.chave] || c.titulo);
    b.setAttribute('role', 'tab');
    b.setAttribute('aria-selected', i === atual ? 'true' : 'false');
    b.addEventListener('click', () => { atual = i; desenhar(); });
  });
  const cen = DADOS.cenarios[atual];
  document.getElementById('descricao').textContent = (DADOS.cenarios.length > 1 ? '' : cen.titulo + '. ') +
    (DESCRICOES[cen.chave] || '');
  const c = document.getElementById('conteudo');
  c.textContent = '';
  destaque(c, cen);
  graficoEscritas(c, cen);
  graficoVida(c, cen);
  graficoMapas(c, cen);
  graficoHistogramas(c, cen);
  graficoEvolucao(c, cen);
  tabela(c, cen);
  esconderDica();
}

const glossario = document.getElementById('glossario');
for (const [termo, def] of GLOSSARIO) { html('dt', '', glossario, termo); html('dd', '', glossario, def); }
document.getElementById('rodape').textContent = 'Página gerada pelo simulador (' + DADOS.comando + '). A semente ' +
  'das cargas é fixa: rodar de novo produz os mesmos números.';
let espera;
window.addEventListener('resize', () => { clearTimeout(espera); espera = setTimeout(desenhar, 150); });
if (escuro.addEventListener) escuro.addEventListener('change', desenhar);
desenhar();
</script>
</body>
</html>
)PI4"
};
