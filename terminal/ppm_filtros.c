/*
 * =============================================================
 *  ppm_filtros.c
 *  Manipulador de Imagens PPM (variante P3)
 *
 *  Funcionalidades:
 *    - Leitura e escrita de arquivos PPM P3
 *    - 6 filtros de cor:
 *        1. Negativo       4. Ajuste de Canal
 *        2. Brilho         5. Escala de Cinza
 *        3. Contraste      6. Limiarizacao
 *    - 6 filtros posicionais:
 *        7. Flip Vertical   10. Rotacao 180°
 *        8. Flip Horizontal 11. Rotacao 270°
 *        9. Rotacao 90°     12. Mosaico
 *    - Convolucao com nucleo NxN livre (minimo 3x3)
 *    - Selecao de regiao (recorte) opcional em todos os filtros
 *
 *  Compilacao:
 *    gcc -o ppm_filtros ppm_filtros.c -lm
 *
 *  Uso:
 *    Linux/Mac: ./ppm_filtros
 *    Windows:   .\ppm_filtros.exe
 * =============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================
 *  ESTRUTURAS DE DADOS
 * ============================================================= */

/* Representa um unico pixel com tres canais de cor (RGB). */
typedef struct {
    unsigned char r, g, b;
} Pixel;

/*
 * Representa uma imagem PPM completa.
 * pixels[linha][coluna] — matriz alocada dinamicamente.
 */
typedef struct {
    int    largura;   /* numero de colunas */
    int    altura;    /* numero de linhas  */
    int    max_cor;   /* valor maximo por canal (normalmente 255) */
    Pixel **pixels;
} Imagem;

/* =============================================================
 *  FUNCOES AUXILIARES
 * ============================================================= */

/*
 * Limita 'valor' ao intervalo [lo, hi].
 * Evita overflow nos canais apos aplicacao de filtros.
 */
int clamp(int valor, int lo, int hi) {
    if (valor < lo) return lo;
    if (valor > hi) return hi;
    return valor;
}

/* Aloca matriz de pixels (altura x largura). Encerra em falha. */
Pixel **alocar_pixels(int altura, int largura) {
    Pixel **p = (Pixel **)malloc(altura * sizeof(Pixel *));
    if (!p) { fprintf(stderr, "Erro: memoria insuficiente.\n"); exit(1); }
    for (int i = 0; i < altura; i++) {
        p[i] = (Pixel *)malloc(largura * sizeof(Pixel));
        if (!p[i]) { fprintf(stderr, "Erro: memoria insuficiente.\n"); exit(1); }
    }
    return p;
}

/* Libera memoria de uma matriz de pixels. */
void liberar_pixels(Pixel **p, int altura) {
    for (int i = 0; i < altura; i++) free(p[i]);
    free(p);
}

/*
 * Retorna copia profunda de 'src' com nova matriz de pixels.
 * Usada pela convolucao para nao ler pixels ja modificados.
 */
Imagem copiar_imagem(const Imagem *src) {
    Imagem dst;
    dst.largura = src->largura;
    dst.altura  = src->altura;
    dst.max_cor = src->max_cor;
    dst.pixels  = alocar_pixels(dst.altura, dst.largura);
    for (int i = 0; i < dst.altura; i++)
        for (int j = 0; j < dst.largura; j++)
            dst.pixels[i][j] = src->pixels[i][j];
    return dst;
}

/* Libera a memoria da matriz de pixels de uma imagem. */
void liberar_imagem(Imagem *img) {
    liberar_pixels(img->pixels, img->altura);
    img->pixels = NULL;
}

/* =============================================================
 *  LEITURA E ESCRITA PPM P3
 * ============================================================= */

/*
 * Le um arquivo PPM P3 do caminho indicado e retorna a Imagem.
 *
 * Formato esperado (sem comentarios '#'):
 *   linha 1: "P3"
 *   linha 2: largura altura
 *   linha 3: max_cor
 *   restante: triplas R G B por pixel, da esquerda para direita,
 *             de cima para baixo.
 */
Imagem ler_ppm(const char *caminho) {
    Imagem img;

    FILE *f = fopen(caminho, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s'.\n"
                        "Verifique se o arquivo esta na mesma pasta do executavel.\n",
                caminho);
        exit(1);
    }

    char magic[4];
    fscanf(f, "%3s", magic);
    if (strcmp(magic, "P3") != 0) {
        fprintf(stderr, "Erro: '%s' nao e um arquivo PPM P3 valido.\n", caminho);
        fclose(f);
        exit(1);
    }

    fscanf(f, "%d %d", &img.largura, &img.altura);
    fscanf(f, "%d",    &img.max_cor);

    img.pixels = alocar_pixels(img.altura, img.largura);
    for (int i = 0; i < img.altura; i++) {
        for (int j = 0; j < img.largura; j++) {
            int r, g, b;
            if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) {
                fprintf(stderr, "Erro: dados de pixel incompletos em '%s'.\n", caminho);
                fclose(f);
                exit(1);
            }
            img.pixels[i][j].r = (unsigned char)r;
            img.pixels[i][j].g = (unsigned char)g;
            img.pixels[i][j].b = (unsigned char)b;
        }
    }

    fclose(f);
    return img;
}

/*
 * Salva a imagem no formato PPM P3.
 * Cada linha do arquivo corresponde a uma linha da imagem,
 * com os valores RGB separados por espacos.
 */
void salvar_ppm(const Imagem *img, const char *caminho) {
    FILE *f = fopen(caminho, "w");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n", caminho);
        return;
    }

    fprintf(f, "P3\n%d %d\n%d\n", img->largura, img->altura, img->max_cor);

    for (int i = 0; i < img->altura; i++) {
        for (int j = 0; j < img->largura; j++) {
            fprintf(f, "%d %d %d",
                img->pixels[i][j].r,
                img->pixels[i][j].g,
                img->pixels[i][j].b);
            if (j < img->largura - 1) fprintf(f, "   ");
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("Imagem salva em '%s'.\n", caminho);
}

/* =============================================================
 *  SELECAO DE REGIAO (RECORTE)
 * ============================================================= */

/*
 * Pergunta ao usuario se deseja aplicar o filtro em uma regiao
 * especifica. Se sim, solicita as coordenadas do retangulo:
 *   (x1, y1) = canto superior-esquerdo
 *   (x2, y2) = canto inferior-direito (ambos inclusivos)
 *
 * Os valores sao ajustados automaticamente para ficar dentro
 * dos limites validos da imagem. Se o usuario escolher a imagem
 * toda, preenche as coordenadas com os extremos.
 *
 * Retorna 1 se houver selecao, 0 se for imagem toda.
 */
int perguntar_recorte(const Imagem *img,
                      int *x1, int *y1, int *x2, int *y2) {
    char resp;
    printf("Aplicar em uma regiao especifica? (s/n): ");
    scanf(" %c", &resp);

    if (resp != 's' && resp != 'S') {
        *x1 = 0;                *y1 = 0;
        *x2 = img->largura - 1; *y2 = img->altura - 1;
        return 0;
    }

    printf("Dimensoes: %d colunas x %d linhas\n", img->largura, img->altura);
    printf("Coluna inicial (0..%d): ",        img->largura - 1); scanf("%d", x1);
    printf("Linha inicial  (0..%d): ",        img->altura  - 1); scanf("%d", y1);
    printf("Coluna final   (%d..%d): ", *x1,  img->largura - 1); scanf("%d", x2);
    printf("Linha final    (%d..%d): ", *y1,  img->altura  - 1); scanf("%d", y2);

    *x1 = clamp(*x1, 0,   img->largura - 1);
    *y1 = clamp(*y1, 0,   img->altura  - 1);
    *x2 = clamp(*x2, *x1, img->largura - 1);
    *y2 = clamp(*y2, *y1, img->altura  - 1);
    return 1;
}

/* =============================================================
 *  FILTROS DE COR
 *
 *  Todos recebem a imagem e as coordenadas da regiao (x1,y1)
 *  ate (x2,y2), ambos inclusivos, e modificam apenas os pixels
 *  dentro dessa area.
 * ============================================================= */

/*
 * NEGATIVO
 * Inverte cada canal: novo = max_cor - canal.
 * Pixels claros ficam escuros e vice-versa.
 * Cores tornam-se seus complementares (ex.: vermelho vira ciano).
 */
void filtro_negativo(Imagem *img, int x1, int y1, int x2, int y2) {
    int M = img->max_cor;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            img->pixels[i][j].r = M - img->pixels[i][j].r;
            img->pixels[i][j].g = M - img->pixels[i][j].g;
            img->pixels[i][j].b = M - img->pixels[i][j].b;
        }
}

/*
 * BRILHO
 * Soma 'delta' a cada canal: novo = clamp(canal + delta, 0, max).
 * Delta positivo ilumina; delta negativo escurece.
 */
void filtro_brilho(Imagem *img, int delta, int x1, int y1, int x2, int y2) {
    int M = img->max_cor;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            img->pixels[i][j].r = (unsigned char)clamp(img->pixels[i][j].r + delta, 0, M);
            img->pixels[i][j].g = (unsigned char)clamp(img->pixels[i][j].g + delta, 0, M);
            img->pixels[i][j].b = (unsigned char)clamp(img->pixels[i][j].b + delta, 0, M);
        }
}

/*
 * CONTRASTE
 * Ajusta em relacao ao ponto medio (mid = max_cor / 2):
 *   novo = clamp(mid + fator * (canal - mid), 0, max)
 * Fator > 1 aumenta contraste; 0 < fator < 1 reduz.
 */
void filtro_contraste(Imagem *img, float fator, int x1, int y1, int x2, int y2) {
    int   M   = img->max_cor;
    float mid = M / 2.0f;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            img->pixels[i][j].r = (unsigned char)clamp((int)(mid + fator * (img->pixels[i][j].r - mid)), 0, M);
            img->pixels[i][j].g = (unsigned char)clamp((int)(mid + fator * (img->pixels[i][j].g - mid)), 0, M);
            img->pixels[i][j].b = (unsigned char)clamp((int)(mid + fator * (img->pixels[i][j].b - mid)), 0, M);
        }
}

/*
 * AJUSTE DE CANAL
 * Multiplica apenas um canal (R=0, G=1, B=2) por 'fator':
 *   novo = clamp(canal * fator, 0, max)
 * Fator 0.0 zera o canal; fator 2.0 dobra sua intensidade.
 */
void filtro_canal(Imagem *img, int canal, float fator, int x1, int y1, int x2, int y2) {
    int M = img->max_cor;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            Pixel *p = &img->pixels[i][j];
            if      (canal == 0) p->r = (unsigned char)clamp((int)(p->r * fator), 0, M);
            else if (canal == 1) p->g = (unsigned char)clamp((int)(p->g * fator), 0, M);
            else                 p->b = (unsigned char)clamp((int)(p->b * fator), 0, M);
        }
}

/*
 * ESCALA DE CINZA
 * Converte com ponderacao perceptual ITU-R BT.601:
 *   cinza = 0.299*R + 0.587*G + 0.114*B
 * Os pesos refletem a sensibilidade do olho humano a cada cor.
 */
void filtro_cinza(Imagem *img, int x1, int y1, int x2, int y2) {
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            Pixel *p = &img->pixels[i][j];
            unsigned char c = (unsigned char)(0.299f * p->r
                                            + 0.587f * p->g
                                            + 0.114f * p->b);
            p->r = p->g = p->b = c;
        }
}

/*
 * LIMIARIZACAO (THRESHOLD)
 * Binariza a imagem com base em um limiar:
 *   media = (R + G + B) / 3
 *   media >= limiar -> branco (max_cor)
 *   media <  limiar -> preto  (0)
 */
void filtro_limiar(Imagem *img, int limiar, int x1, int y1, int x2, int y2) {
    int M = img->max_cor;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            Pixel *p     = &img->pixels[i][j];
            int    media = ((int)p->r + p->g + p->b) / 3;
            unsigned char val = (media >= limiar) ? (unsigned char)M : 0;
            p->r = p->g = p->b = val;
        }
}

/* =============================================================
 *  FILTROS POSICIONAIS
 *
 *  Reorganizam a posicao dos pixels dentro da regiao indicada.
 * ============================================================= */

/*
 * FLIP VERTICAL
 * Inverte a ordem das linhas: a primeira troca com a ultima,
 * a segunda com a penultima, e assim por diante.
 * Resultado: imagem refletida no eixo horizontal.
 */
void filtro_flip_vertical(Imagem *img, int x1, int y1, int x2, int y2) {
    int topo = y1, base = y2;
    while (topo < base) {
        for (int j = x1; j <= x2; j++) {
            Pixel tmp             = img->pixels[topo][j];
            img->pixels[topo][j] = img->pixels[base][j];
            img->pixels[base][j] = tmp;
        }
        topo++; base--;
    }
}

/*
 * FLIP HORIZONTAL
 * Inverte a ordem das colunas em cada linha da regiao.
 * Resultado: imagem espelhada no eixo vertical.
 */
void filtro_flip_horizontal(Imagem *img, int x1, int y1, int x2, int y2) {
    int esq = x1, dir = x2;
    while (esq < dir) {
        for (int i = y1; i <= y2; i++) {
            Pixel tmp            = img->pixels[i][esq];
            img->pixels[i][esq] = img->pixels[i][dir];
            img->pixels[i][dir] = tmp;
        }
        esq++; dir--;
    }
}

/*
 * ROTACAO 90 GRAUS HORARIO
 * Copia a regiao para matriz temporaria e reposiciona:
 *   tmp[i][j] -> img[y1+j][x1+(lado-1-i)]
 * Para regioes nao quadradas, opera sobre o maior quadrado
 * contido (lado = min(largura, altura) da regiao).
 */
void filtro_rotacao90(Imagem *img, int x1, int y1, int x2, int y2) {
    int rw = x2 - x1 + 1;
    int rh = y2 - y1 + 1;

    Pixel **tmp = alocar_pixels(rh, rw);
    for (int i = 0; i < rh; i++)
        for (int j = 0; j < rw; j++)
            tmp[i][j] = img->pixels[y1 + i][x1 + j];

    int lado = (rw < rh) ? rw : rh;
    for (int i = 0; i < lado; i++)
        for (int j = 0; j < lado; j++)
            img->pixels[y1 + j][x1 + (lado - 1 - i)] = tmp[i][j];

    liberar_pixels(tmp, rh);
}

/*
 * ROTACAO 180 GRAUS
 * Equivale a flip vertical seguido de flip horizontal.
 */
void filtro_rotacao180(Imagem *img, int x1, int y1, int x2, int y2) {
    filtro_flip_vertical  (img, x1, y1, x2, y2);
    filtro_flip_horizontal(img, x1, y1, x2, y2);
}

/*
 * ROTACAO 270 GRAUS HORARIO (= 90 graus anti-horario)
 * Aplica filtro_rotacao90 tres vezes consecutivas.
 */
void filtro_rotacao270(Imagem *img, int x1, int y1, int x2, int y2) {
    filtro_rotacao90(img, x1, y1, x2, y2);
    filtro_rotacao90(img, x1, y1, x2, y2);
    filtro_rotacao90(img, x1, y1, x2, y2);
}

/*
 * MOSAICO (PIXELIZACAO)
 * Divide a regiao em blocos de tam_bloco x tam_bloco pixels.
 * Cada bloco e substituido pela media de cor dos seus pixels.
 * Cria efeito de pixelizacao semelhante ao usado para ocultar rostos.
 */
void filtro_mosaico(Imagem *img, int tam_bloco, int x1, int y1, int x2, int y2) {
    for (int bi = y1; bi <= y2; bi += tam_bloco) {
        for (int bj = x1; bj <= x2; bj += tam_bloco) {
            int iy2 = (bi + tam_bloco - 1 < y2) ? bi + tam_bloco - 1 : y2;
            int jx2 = (bj + tam_bloco - 1 < x2) ? bj + tam_bloco - 1 : x2;

            long sr = 0, sg = 0, sb = 0, cnt = 0;
            for (int i = bi; i <= iy2; i++)
                for (int j = bj; j <= jx2; j++) {
                    sr += img->pixels[i][j].r;
                    sg += img->pixels[i][j].g;
                    sb += img->pixels[i][j].b;
                    cnt++;
                }

            unsigned char mr = (unsigned char)(sr / cnt);
            unsigned char mg = (unsigned char)(sg / cnt);
            unsigned char mb = (unsigned char)(sb / cnt);

            for (int i = bi; i <= iy2; i++)
                for (int j = bj; j <= jx2; j++) {
                    img->pixels[i][j].r = mr;
                    img->pixels[i][j].g = mg;
                    img->pixels[i][j].b = mb;
                }
        }
    }
}

/* =============================================================
 *  CONVOLUCAO COM NUCLEO NxN
 * ============================================================= */

/*
 * Aplica um nucleo de convolucao de tamanho n x n sobre a regiao.
 *
 * Para cada pixel (i,j) da regiao:
 *   novo(i,j) = soma(nucleo[ki][kj] * copia[i+ki-meia][j+kj-meia])
 *               / soma_dos_pesos_do_nucleo
 *
 * - Clamp padding nas bordas: indices fora dos limites usam o pixel
 *   de borda mais proximo, evitando artefatos.
 * - Uma copia da imagem e usada como fonte de leitura, garantindo
 *   que pixels ja processados nao influenciem os seguintes.
 * - Se a soma dos pesos for zero (ex.: deteccao de borda), o divisor
 *   e forcado a 1.0 para evitar divisao por zero.
 *
 * Exemplos de nucleos:
 *   Blur 3x3:          todos = 1      (media dos vizinhos)
 *   Nitidez 3x3:       centro=5, vizinhos diretos=-1, cantos=0
 *   Detec. borda 3x3:  centro=8, todos os outros=-1
 */
void aplicar_convolucao(Imagem *img,
                        float **nucleo, int n,
                        int x1, int y1, int x2, int y2) {
    int   meia = n / 2;
    int   M    = img->max_cor;

    float soma_nucleo = 0.0f;
    for (int ki = 0; ki < n; ki++)
        for (int kj = 0; kj < n; kj++)
            soma_nucleo += nucleo[ki][kj];
    if (soma_nucleo == 0.0f) soma_nucleo = 1.0f;

    Imagem copia = copiar_imagem(img);

    for (int i = y1; i <= y2; i++) {
        for (int j = x1; j <= x2; j++) {
            float acc_r = 0.0f, acc_g = 0.0f, acc_b = 0.0f;
            for (int ki = 0; ki < n; ki++) {
                for (int kj = 0; kj < n; kj++) {
                    int   ni = clamp(i + ki - meia, 0, img->altura  - 1);
                    int   nj = clamp(j + kj - meia, 0, img->largura - 1);
                    float w  = nucleo[ki][kj];
                    acc_r += w * copia.pixels[ni][nj].r;
                    acc_g += w * copia.pixels[ni][nj].g;
                    acc_b += w * copia.pixels[ni][nj].b;
                }
            }
            img->pixels[i][j].r = (unsigned char)clamp((int)(acc_r / soma_nucleo), 0, M);
            img->pixels[i][j].g = (unsigned char)clamp((int)(acc_g / soma_nucleo), 0, M);
            img->pixels[i][j].b = (unsigned char)clamp((int)(acc_b / soma_nucleo), 0, M);
        }
    }

    liberar_imagem(&copia);
}

/* =============================================================
 *  MENU E PROGRAMA PRINCIPAL
 * ============================================================= */

void exibir_menu(void) {
    printf("\n========== MENU ==========\n");
    printf("--- Filtros de Cor ---\n");
    printf(" 1. Negativo\n");
    printf(" 2. Brilho\n");
    printf(" 3. Contraste\n");
    printf(" 4. Ajuste de Canal (R/G/B)\n");
    printf(" 5. Escala de Cinza\n");
    printf(" 6. Limiarizacao\n");
    printf("--- Filtros Posicionais ---\n");
    printf(" 7. Flip Vertical\n");
    printf(" 8. Flip Horizontal\n");
    printf(" 9. Rotacao 90 graus Horario\n");
    printf("10. Rotacao 180 graus\n");
    printf("11. Rotacao 270 graus Horario\n");
    printf("12. Mosaico (Pixelizacao)\n");
    printf("--- Convolucao ---\n");
    printf("13. Convolucao com Nucleo Personalizado\n");
    printf("--- Arquivo ---\n");
    printf("14. Salvar imagem\n");
    printf(" 0. Sair\n");
    printf("==========================\n");
    printf("Opcao: ");
}

int main(void) {
    char caminho[512];

    printf("=== Manipulador de Imagens PPM P3 ===\n");
    printf("Arquivo PPM de entrada: ");

    /* fgets aceita caminhos com espacos; strcspn remove o '\n' final */
    if (!fgets(caminho, sizeof(caminho), stdin)) {
        fprintf(stderr, "Erro ao ler o nome do arquivo.\n");
        return 1;
    }
    caminho[strcspn(caminho, "\n")] = '\0';

    Imagem img = ler_ppm(caminho);
    printf("Imagem carregada: %d x %d pixels, max_cor=%d\n",
           img.largura, img.altura, img.max_cor);

    int opcao;
    do {
        exibir_menu();
        scanf("%d", &opcao);

        int x1, y1, x2, y2;

        switch (opcao) {

        /* ── Filtros de Cor ── */

        case 1:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_negativo(&img, x1, y1, x2, y2);
            printf("Negativo aplicado.\n");
            break;

        case 2: {
            int delta;
            printf("Delta de brilho (negativo escurece, positivo ilumina): ");
            scanf("%d", &delta);
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_brilho(&img, delta, x1, y1, x2, y2);
            printf("Brilho aplicado.\n");
            break;
        }

        case 3: {
            float fator;
            printf("Fator de contraste (ex: 1.5 aumenta, 0.5 reduz): ");
            scanf("%f", &fator);
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_contraste(&img, fator, x1, y1, x2, y2);
            printf("Contraste aplicado.\n");
            break;
        }

        case 4: {
            int   canal;
            float fator;
            printf("Canal (0=R, 1=G, 2=B): ");
            scanf("%d", &canal);
            canal = clamp(canal, 0, 2);
            printf("Fator multiplicador (ex: 0.0 zera, 2.0 dobra): ");
            scanf("%f", &fator);
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_canal(&img, canal, fator, x1, y1, x2, y2);
            printf("Ajuste de canal aplicado.\n");
            break;
        }

        case 5:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_cinza(&img, x1, y1, x2, y2);
            printf("Escala de cinza aplicada.\n");
            break;

        case 6: {
            int limiar;
            printf("Limiar (0..%d): ", img.max_cor);
            scanf("%d", &limiar);
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_limiar(&img, limiar, x1, y1, x2, y2);
            printf("Limiarizacao aplicada.\n");
            break;
        }

        /* ── Filtros Posicionais ── */

        case 7:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_flip_vertical(&img, x1, y1, x2, y2);
            printf("Flip vertical aplicado.\n");
            break;

        case 8:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_flip_horizontal(&img, x1, y1, x2, y2);
            printf("Flip horizontal aplicado.\n");
            break;

        case 9:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_rotacao90(&img, x1, y1, x2, y2);
            printf("Rotacao 90 graus horario aplicada.\n");
            break;

        case 10:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_rotacao180(&img, x1, y1, x2, y2);
            printf("Rotacao 180 graus aplicada.\n");
            break;

        case 11:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_rotacao270(&img, x1, y1, x2, y2);
            printf("Rotacao 270 graus horario aplicada.\n");
            break;

        case 12: {
            int tam;
            printf("Tamanho do bloco (ex: 8, 16): ");
            scanf("%d", &tam);
            if (tam < 1) tam = 1;
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_mosaico(&img, tam, x1, y1, x2, y2);
            printf("Mosaico aplicado.\n");
            break;
        }

        /* ── Convolucao ── */

        case 13: {
            int n;
            printf("Tamanho do nucleo NxN (minimo 3, deve ser impar): ");
            scanf("%d", &n);
            if (n < 3) n = 3;
            if (n % 2 == 0) n++;

            float **nucleo = (float **)malloc(n * sizeof(float *));
            for (int i = 0; i < n; i++)
                nucleo[i] = (float *)malloc(n * sizeof(float));

            printf("Digite os %d valores do nucleo, linha por linha:\n", n * n);
            for (int i = 0; i < n; i++) {
                printf("  Linha %d: ", i);
                for (int j = 0; j < n; j++)
                    scanf("%f", &nucleo[i][j]);
            }

            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            aplicar_convolucao(&img, nucleo, n, x1, y1, x2, y2);

            for (int i = 0; i < n; i++) free(nucleo[i]);
            free(nucleo);
            printf("Convolucao aplicada.\n");
            break;
        }

        /* ── Arquivo ── */

        case 14: {
            char saida[512];
            /* Consome o '\n' residual do buffer antes do fgets */
            int c; while ((c = getchar()) != '\n' && c != EOF);
            printf("Nome do arquivo de saida (ex: saida.ppm): ");
            fgets(saida, sizeof(saida), stdin);
            saida[strcspn(saida, "\n")] = '\0';
            salvar_ppm(&img, saida);
            break;
        }

        case 0:
            printf("Encerrando.\n");
            break;

        default:
            printf("Opcao invalida. Tente novamente.\n");
        }

    } while (opcao != 0);

    liberar_imagem(&img);
    return 0;
}
