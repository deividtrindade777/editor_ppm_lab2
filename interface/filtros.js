/* ============================================================
   filtros.js — Logica de Filtros e Interface PPM Editor
   
   Conteudo:
     1. Estado global
     2. Leitura e escrita PPM P3
     3. Renderizacao no canvas
     4. Historico (undo)
     5. Funcao auxiliar clamp
     6. Filtros de cor
     7. Filtros posicionais
     8. Convolucao
     9. Construcao do modal
    10. Eventos dos botoes
   ============================================================ */

/* ============================================================
   1. ESTADO GLOBAL
   ============================================================ */

let imgData  = null;   /* ImageData atual (editavel) */
let original = null;   /* ImageData original para reset */
let history  = [];     /* pilha de estados anteriores (undo) */
let W = 0, H = 0, maxColor = 255;
let pendingFilter = null;

const canvas  = document.getElementById('canvas');
const ctx     = canvas.getContext('2d');
const fileInput = document.getElementById('fileInput');

/* ============================================================
   2. LEITURA E ESCRITA PPM P3
   ============================================================ */

/*
 * Converte o texto de um arquivo PPM P3 em um ImageData.
 * Escala os valores de [0, maxColor] para [0, 255].
 */
function parsePPM(text) {
    const tokens = text.trim().split(/\s+/);
    let i = 0;

    if (tokens[i++] !== 'P3') throw new Error('Arquivo nao e PPM P3');

    const w    = parseInt(tokens[i++]);
    const h    = parseInt(tokens[i++]);
    const maxC = parseInt(tokens[i++]);

    const data = new Uint8ClampedArray(w * h * 4);
    for (let p = 0; p < w * h; p++) {
        data[p*4]   = Math.round(parseInt(tokens[i++]) / maxC * 255);
        data[p*4+1] = Math.round(parseInt(tokens[i++]) / maxC * 255);
        data[p*4+2] = Math.round(parseInt(tokens[i++]) / maxC * 255);
        data[p*4+3] = 255; /* canal alpha sempre opaco */
    }

    return { w, h, maxC, imageData: new ImageData(data, w, h) };
}

/*
 * Serializa o ImageData atual de volta ao formato PPM P3.
 * Escala os valores de [0, 255] para [0, maxColor].
 */
function serializePPM() {
    const d = imgData.data;
    let out = `P3\n${W} ${H}\n${maxColor}\n`;

    for (let i = 0; i < H; i++) {
        const row = [];
        for (let j = 0; j < W; j++) {
            const idx = (i * W + j) * 4;
            const r = Math.round(d[idx]   / 255 * maxColor);
            const g = Math.round(d[idx+1] / 255 * maxColor);
            const b = Math.round(d[idx+2] / 255 * maxColor);
            row.push(`${r} ${g} ${b}`);
        }
        out += row.join('   ') + '\n';
    }
    return out;
}

/* ============================================================
   3. RENDERIZACAO NO CANVAS
   ============================================================ */

function render() {
    canvas.width  = W;
    canvas.height = H;
    ctx.putImageData(imgData, 0, 0);
    document.getElementById('imgInfo').textContent =
        `${W} x ${H} px  |  max_cor: ${maxColor}`;
}

/* Carrega o arquivo PPM selecionado pelo usuario */
fileInput.addEventListener('change', e => {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = ev => {
        try {
            const res = parsePPM(ev.target.result);
            W = res.w; H = res.h; maxColor = res.maxC;
            imgData  = res.imageData;
            original = new ImageData(new Uint8ClampedArray(imgData.data), W, H);
            history  = [];

            render();
            document.getElementById('emptyState').style.display  = 'none';
            document.getElementById('canvasWrap').style.display  = 'block';
            enableButtons(true);
            toast(`Imagem carregada: ${file.name}`);
        } catch (err) {
            toast(err.message, true);
        }
    };
    reader.readAsText(file);
});

function enableButtons(on) {
    document.querySelectorAll('.filter-btn, #btnSave, #btnReset')
        .forEach(b => b.disabled = !on);
    document.getElementById('btnUndo').disabled = (history.length === 0);
}

/* ============================================================
   4. HISTORICO (UNDO)
   ============================================================ */

/* Salva o estado atual antes de aplicar um filtro */
function pushHistory() {
    history.push(new Uint8ClampedArray(imgData.data));
    document.getElementById('btnUndo').disabled = false;
}

document.getElementById('btnUndo').addEventListener('click', () => {
    if (!history.length) return;
    imgData.data.set(history.pop());
    render();
    document.getElementById('btnUndo').disabled = (history.length === 0);
    toast('Acao desfeita');
});

/* ============================================================
   5. FUNCAO AUXILIAR CLAMP
   ============================================================ */

/* Limita 'v' ao intervalo [lo, hi] */
function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
}

/* ============================================================
   6. FILTROS DE COR
   Todos operam diretamente sobre imgData.data (Uint8ClampedArray).
   Os algoritmos sao identicos aos do codigo C.
   ============================================================ */

/*
 * Aplica uma funcao 'fn(data, indice)' a cada pixel da regiao.
 * Salva o estado anterior no historico antes de modificar.
 */
function applyToRegion(fn, x1, y1, x2, y2) {
    pushHistory();
    const d = imgData.data;
    for (let i = y1; i <= y2; i++)
        for (let j = x1; j <= x2; j++)
            fn(d, (i * W + j) * 4);
    render();
}

/* Negativo: novo = 255 - canal */
function doNegativo(x1, y1, x2, y2) {
    applyToRegion((d, p) => {
        d[p]   = 255 - d[p];
        d[p+1] = 255 - d[p+1];
        d[p+2] = 255 - d[p+2];
    }, x1, y1, x2, y2);
}

/* Brilho: novo = clamp(canal + delta, 0, 255) */
function doBrilho(delta, x1, y1, x2, y2) {
    applyToRegion((d, p) => {
        d[p]   = clamp(d[p]   + delta, 0, 255);
        d[p+1] = clamp(d[p+1] + delta, 0, 255);
        d[p+2] = clamp(d[p+2] + delta, 0, 255);
    }, x1, y1, x2, y2);
}

/* Contraste: novo = clamp(mid + fator * (canal - mid), 0, 255) */
function doContraste(fator, x1, y1, x2, y2) {
    const mid = 127.5;
    applyToRegion((d, p) => {
        d[p]   = clamp(Math.round(mid + fator * (d[p]   - mid)), 0, 255);
        d[p+1] = clamp(Math.round(mid + fator * (d[p+1] - mid)), 0, 255);
        d[p+2] = clamp(Math.round(mid + fator * (d[p+2] - mid)), 0, 255);
    }, x1, y1, x2, y2);
}

/* Ajuste de canal: multiplica apenas R, G ou B pelo fator */
function doCanal(canal, fator, x1, y1, x2, y2) {
    applyToRegion((d, p) => {
        d[p + canal] = clamp(Math.round(d[p + canal] * fator), 0, 255);
    }, x1, y1, x2, y2);
}

/* Escala de cinza: cinza = 0.299R + 0.587G + 0.114B */
function doCinza(x1, y1, x2, y2) {
    applyToRegion((d, p) => {
        const c = Math.round(0.299 * d[p] + 0.587 * d[p+1] + 0.114 * d[p+2]);
        d[p] = d[p+1] = d[p+2] = c;
    }, x1, y1, x2, y2);
}

/* Limiarizacao: media >= limiar -> branco; caso contrario -> preto */
function doLimiar(limiar, x1, y1, x2, y2) {
    applyToRegion((d, p) => {
        const media = (d[p] + d[p+1] + d[p+2]) / 3;
        const v = media >= limiar ? 255 : 0;
        d[p] = d[p+1] = d[p+2] = v;
    }, x1, y1, x2, y2);
}

/* ============================================================
   7. FILTROS POSICIONAIS
   ============================================================ */

/* Flip Vertical: troca linhas simetricas */
function doFlipV(x1, y1, x2, y2) {
    pushHistory();
    const d = imgData.data;
    let topo = y1, base = y2;
    while (topo < base) {
        for (let j = x1; j <= x2; j++) {
            const pi = (topo * W + j) * 4;
            const pb = (base * W + j) * 4;
            for (let c = 0; c < 4; c++) {
                const t = d[pi+c]; d[pi+c] = d[pb+c]; d[pb+c] = t;
            }
        }
        topo++; base--;
    }
    render();
}

/* Flip Horizontal: troca colunas simetricas em cada linha */
function doFlipH(x1, y1, x2, y2) {
    pushHistory();
    const d = imgData.data;
    for (let i = y1; i <= y2; i++) {
        let l = x1, r = x2;
        while (l < r) {
            const pl = (i * W + l) * 4;
            const pr = (i * W + r) * 4;
            for (let c = 0; c < 4; c++) {
                const t = d[pl+c]; d[pl+c] = d[pr+c]; d[pr+c] = t;
            }
            l++; r--;
        }
    }
    render();
}

/* Rotacao: aplica rot90 'graus/90' vezes */
function doRotacao(graus, x1, y1, x2, y2) {
    pushHistory();
    const vezes = graus / 90;
    for (let t = 0; t < vezes; t++) rot90once(x1, y1, x2, y2);
    render();
}

/* Rotacao 90 graus horario sobre o maior quadrado da regiao */
function rot90once(x1, y1, x2, y2) {
    const d  = imgData.data;
    const rw = x2 - x1 + 1;
    const rh = y2 - y1 + 1;

    /* Copia a regiao original */
    const tmp = [];
    for (let i = 0; i < rh; i++) {
        tmp[i] = [];
        for (let j = 0; j < rw; j++) {
            const p = ((y1 + i) * W + (x1 + j)) * 4;
            tmp[i][j] = [d[p], d[p+1], d[p+2], d[p+3]];
        }
    }

    /* Aplica no maior quadrado */
    const lado = Math.min(rw, rh);
    for (let i = 0; i < lado; i++) {
        for (let j = 0; j < lado; j++) {
            const tp = ((y1 + j) * W + (x1 + (lado - 1 - i))) * 4;
            [d[tp], d[tp+1], d[tp+2], d[tp+3]] = tmp[i][j];
        }
    }
}

/* Mosaico: substitui cada bloco pela media de cor dos seus pixels */
function doMosaico(tam, x1, y1, x2, y2) {
    pushHistory();
    const d = imgData.data;
    for (let bi = y1; bi <= y2; bi += tam) {
        for (let bj = x1; bj <= x2; bj += tam) {
            const iy2 = Math.min(bi + tam - 1, y2);
            const jx2 = Math.min(bj + tam - 1, x2);
            let sr = 0, sg = 0, sb = 0, cnt = 0;

            for (let i = bi; i <= iy2; i++) {
                for (let j = bj; j <= jx2; j++) {
                    const p = (i * W + j) * 4;
                    sr += d[p]; sg += d[p+1]; sb += d[p+2]; cnt++;
                }
            }

            const mr = Math.round(sr / cnt);
            const mg = Math.round(sg / cnt);
            const mb = Math.round(sb / cnt);

            for (let i = bi; i <= iy2; i++) {
                for (let j = bj; j <= jx2; j++) {
                    const p = (i * W + j) * 4;
                    d[p] = mr; d[p+1] = mg; d[p+2] = mb;
                }
            }
        }
    }
    render();
}

/* ============================================================
   8. CONVOLUCAO
   ============================================================ */

/*
 * Aplica nucleo NxN sobre a regiao.
 * Usa um snapshot da imagem como fonte para nao contaminar
 * os vizinhos com pixels ja processados.
 * Clamp padding nas bordas (repete o pixel de borda).
 */
function doConvolucao(nucleo, n, x1, y1, x2, y2) {
    pushHistory();
    const d    = imgData.data;
    const meia = Math.floor(n / 2);
    const snap = new Uint8ClampedArray(d); /* copia fonte */

    /* Soma dos pesos para normalizacao */
    let soma = 0;
    for (let ki = 0; ki < n; ki++)
        for (let kj = 0; kj < n; kj++)
            soma += nucleo[ki][kj];
    if (soma === 0) soma = 1;

    for (let i = y1; i <= y2; i++) {
        for (let j = x1; j <= x2; j++) {
            let ar = 0, ag = 0, ab = 0;

            for (let ki = 0; ki < n; ki++) {
                for (let kj = 0; kj < n; kj++) {
                    const ni = clamp(i + ki - meia, 0, H - 1);
                    const nj = clamp(j + kj - meia, 0, W - 1);
                    const w  = nucleo[ki][kj];
                    const sp = (ni * W + nj) * 4;
                    ar += w * snap[sp];
                    ag += w * snap[sp+1];
                    ab += w * snap[sp+2];
                }
            }

            const dp = (i * W + j) * 4;
            d[dp]   = clamp(Math.round(ar / soma), 0, 255);
            d[dp+1] = clamp(Math.round(ag / soma), 0, 255);
            d[dp+2] = clamp(Math.round(ab / soma), 0, 255);
        }
    }
    render();
}

/* ============================================================
   9. CONSTRUCAO DOS MODAIS
   ============================================================ */

/* Retorna o HTML do bloco de selecao de regiao (recorte) */
function cropFields() {
    return `
    <label class="crop-toggle">
        <input type="checkbox" id="useCrop"> Aplicar em regiao especifica
    </label>
    <div class="crop-fields" id="cropFields">
        <div class="field"><label>Coluna inicial → final</label>
            <div class="row">
                <input type="number" id="cx1" value="0"     min="0" max="${W-1}">
                <input type="number" id="cx2" value="${W-1}" min="0" max="${W-1}">
            </div>
        </div>
        <div class="field"><label>Linha inicial → final</label>
            <div class="row">
                <input type="number" id="cy1" value="0"     min="0" max="${H-1}">
                <input type="number" id="cy2" value="${H-1}" min="0" max="${H-1}">
            </div>
        </div>
    </div>`;
}

/* Le as coordenadas do recorte do modal */
function getCrop() {
    const use = document.getElementById('useCrop')?.checked;
    if (!use) return { x1: 0, y1: 0, x2: W-1, y2: H-1 };
    return {
        x1: clamp(parseInt(document.getElementById('cx1').value) || 0, 0, W-1),
        y1: clamp(parseInt(document.getElementById('cy1').value) || 0, 0, H-1),
        x2: clamp(parseInt(document.getElementById('cx2').value) || 0, 0, W-1),
        y2: clamp(parseInt(document.getElementById('cy2').value) || 0, 0, H-1),
    };
}

/* Definicoes de cada filtro: titulo, corpo do modal e funcao apply */
const MODAL_DEFS = {
    negativo:  {
        title: 'NEGATIVO',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doNegativo(c.x1, c.y1, c.x2, c.y2); }
    },
    brilho: {
        title: 'BRILHO',
        body:  () => `
            <div class="field"><label>Delta (negativo escurece, positivo ilumina)</label>
                <input type="number" id="delta" value="30" min="-255" max="255">
            </div>${cropFields()}`,
        apply: () => {
            const delta = parseInt(document.getElementById('delta').value) || 0;
            const c = getCrop(); doBrilho(delta, c.x1, c.y1, c.x2, c.y2);
        }
    },
    contraste: {
        title: 'CONTRASTE',
        body:  () => `
            <div class="field"><label>Fator (ex: 1.5 aumenta, 0.5 reduz)</label>
                <input type="number" id="fator" value="1.5" step="0.1" min="0">
            </div>${cropFields()}`,
        apply: () => {
            const f = parseFloat(document.getElementById('fator').value) || 1;
            const c = getCrop(); doContraste(f, c.x1, c.y1, c.x2, c.y2);
        }
    },
    canal: {
        title: 'AJUSTE DE CANAL',
        body:  () => `
            <div class="field"><label>Canal</label>
                <select id="canal">
                    <option value="0">R — Vermelho</option>
                    <option value="1">G — Verde</option>
                    <option value="2">B — Azul</option>
                </select>
            </div>
            <div class="field"><label>Fator (0 = zera, 2 = dobra)</label>
                <input type="number" id="cfator" value="1.5" step="0.1" min="0">
            </div>${cropFields()}`,
        apply: () => {
            const canal = parseInt(document.getElementById('canal').value);
            const fator = parseFloat(document.getElementById('cfator').value) || 1;
            const c = getCrop(); doCanal(canal, fator, c.x1, c.y1, c.x2, c.y2);
        }
    },
    cinza: {
        title: 'ESCALA DE CINZA',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doCinza(c.x1, c.y1, c.x2, c.y2); }
    },
    limiar: {
        title: 'LIMIARIZACAO',
        body:  () => `
            <div class="field"><label>Limiar (0 a 255)</label>
                <input type="number" id="limiar" value="128" min="0" max="255">
            </div>${cropFields()}`,
        apply: () => {
            const l = parseInt(document.getElementById('limiar').value) || 128;
            const c = getCrop(); doLimiar(l, c.x1, c.y1, c.x2, c.y2);
        }
    },
    flipv: {
        title: 'FLIP VERTICAL',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doFlipV(c.x1, c.y1, c.x2, c.y2); }
    },
    fliph: {
        title: 'FLIP HORIZONTAL',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doFlipH(c.x1, c.y1, c.x2, c.y2); }
    },
    rot90: {
        title: 'ROTACAO 90°',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doRotacao(90,  c.x1, c.y1, c.x2, c.y2); }
    },
    rot180: {
        title: 'ROTACAO 180°',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doRotacao(180, c.x1, c.y1, c.x2, c.y2); }
    },
    rot270: {
        title: 'ROTACAO 270°',
        body:  () => cropFields(),
        apply: () => { const c = getCrop(); doRotacao(270, c.x1, c.y1, c.x2, c.y2); }
    },
    mosaico: {
        title: 'MOSAICO',
        body:  () => `
            <div class="field"><label>Tamanho do bloco (px)</label>
                <input type="number" id="tamBloco" value="8" min="1" max="64">
            </div>${cropFields()}`,
        apply: () => {
            const t = parseInt(document.getElementById('tamBloco').value) || 8;
            const c = getCrop(); doMosaico(t, c.x1, c.y1, c.x2, c.y2);
        }
    },
    convolucao: {
        title: 'CONVOLUCAO',
        body:  () => `
            <div class="field"><label>Tamanho N do nucleo (minimo 3, impar)</label>
                <input type="number" id="kernelN" value="3" min="3" max="9" step="2">
                <button onclick="buildKernel()"
                    style="margin-top:8px;width:100%;padding:8px;background:var(--border);
                           border:none;border-radius:4px;color:var(--text);cursor:pointer;
                           font-family:'Syne',sans-serif">
                    Gerar grade
                </button>
            </div>
            <div class="field" id="kernelWrap"></div>
            ${cropFields()}`,
        apply: () => {
            const n     = parseInt(document.getElementById('kernelN').value) || 3;
            const cells = document.querySelectorAll('.kernel-cell');
            const nucleo = [];
            let k = 0;
            for (let i = 0; i < n; i++) {
                nucleo[i] = [];
                for (let j = 0; j < n; j++)
                    nucleo[i][j] = parseFloat(cells[k++]?.value) || 0;
            }
            const c = getCrop();
            doConvolucao(nucleo, n, c.x1, c.y1, c.x2, c.y2);
        }
    },
};

/* Gera a grade de inputs do nucleo de convolucao */
window.buildKernel = function () {
    let n = parseInt(document.getElementById('kernelN').value) || 3;
    if (n < 3) n = 3;
    if (n % 2 === 0) n++;
    document.getElementById('kernelN').value = n;

    const wrap = document.getElementById('kernelWrap');
    wrap.innerHTML = `
        <label>Valores do nucleo ${n}x${n}</label>
        <div class="kernel-grid" style="grid-template-columns:repeat(${n},1fr)">
            ${'<input type="number" class="kernel-cell" value="1" step="any">'.repeat(n * n)}
        </div>`;
};

/* ============================================================
   10. EVENTOS DOS BOTOES
   ============================================================ */

const modalOverlay = document.getElementById('modalOverlay');
const modalTitle   = document.getElementById('modalTitle');
const modalBody    = document.getElementById('modalBody');

/* Abre o modal ao clicar em um filtro */
document.querySelectorAll('.filter-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const def = MODAL_DEFS[btn.dataset.filter];
        if (!def) return;
        pendingFilter  = def;
        modalTitle.textContent = def.title;
        modalBody.innerHTML    = def.body();

        /* Ativa toggle de recorte */
        const cb = document.getElementById('useCrop');
        if (cb) cb.addEventListener('change', () => {
            document.getElementById('cropFields').classList.toggle('visible', cb.checked);
        });

        modalOverlay.classList.add('open');
    });
});

document.getElementById('btnCancel').addEventListener('click', closeModal);
modalOverlay.addEventListener('click', e => { if (e.target === modalOverlay) closeModal(); });

document.getElementById('btnApply').addEventListener('click', () => {
    if (!pendingFilter) return;
    try {
        pendingFilter.apply();
        toast('Filtro aplicado!');
    } catch (e) {
        toast(e.message, true);
    }
    closeModal();
});

function closeModal() {
    modalOverlay.classList.remove('open');
    pendingFilter = null;
}

/* Salvar PPM */
document.getElementById('btnSave').addEventListener('click', () => {
    const blob = new Blob([serializePPM()], { type: 'text/plain' });
    const a    = document.createElement('a');
    a.href     = URL.createObjectURL(blob);
    a.download = 'resultado.ppm';
    a.click();
    toast('Arquivo salvo!');
});

/* Resetar imagem */
document.getElementById('btnReset').addEventListener('click', () => {
    if (!original) return;
    pushHistory();
    imgData.data.set(original.data);
    render();
    toast('Imagem resetada para o original');
});

/* Toast de notificacao */
let toastTimer;
function toast(msg, err = false) {
    const el = document.getElementById('toast');
    el.textContent = msg;
    el.className   = 'toast show' + (err ? ' err' : '');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { el.className = 'toast'; }, 2800);
}
