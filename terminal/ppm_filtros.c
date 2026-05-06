/*
 * =============================================================
 * ppm_filtros.c
 * Manipulador de Imagens PPM (variante P3)
 *
 * Funcionalidades:
 * - Leitura e escrita de arquivos PPM P3
 * - Selecao de regiao (recorte) opcional em todos os filtros
 * - 3 filtros de cor:
 * 1. Negativo
 * 2. Brilho
 * 3. Escala de Cinza
 * - 3 filtros posicionais:
 * 4. Flip Vertical
 * 5. Flip Horizontal
 * 6. Rotacao 180°
 * - Convolucao com nucleo NxN livre (minimo 3x3)
 *
 * Compilacao:
 * gcc -o ppm_filtros ppm_filtros.c -lm
 *
 * Uso:
 * Linux/Mac: ./ppm_filtros
 * Windows:   .\ppm_filtros.exe
 * =============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* =============================================================
 * ESTRUTURAS DE DADOS
 * ============================================================= */

/* Representa um unico pixel com tres canais de cor (RGB). */
typedef struct {
    unsigned char r, g, b;
} Pixel;

/* Representa uma imagem PPM completa. */
typedef struct {
    int    largura;   
    int    altura;    
    int    max_cor;   
    Pixel **pixels;
} Imagem;

/* =============================================================
 * FUNCOES AUXILIARES
 * ============================================================= */

/* Limita 'valor' ao intervalo [lo, hi]. */
int clamp(int valor, int lo, int hi) {
    if (valor < lo) return lo;
    if (valor > hi) return hi;
    return valor;
}

/* Aloca matriz de pixels (altura x largura). */
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

/* Retorna copia profunda de 'src' com nova matriz de pixels. */
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
 * LEITURA E ESCRITA PPM P3
 * ============================================================= */

Imagem ler_ppm(const char *caminho) {
    Imagem img;
    FILE *f = fopen(caminho, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s'.\n", caminho);
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
                fprintf(stderr, "Erro: dados de pixel incompletos.\n");
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

void salvar_ppm(const Imagem *img, const char *caminho) {
    FILE *f = fopen(caminho, "w");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n", caminho);
        return;
    }

    fprintf(f, "P3\n%d %d\n%d\n", img->largura, img->altura, img->max_cor);

    for (int i = 0; i < img->altura; i++) {
        for (int j = 0; j < img->largura; j++) {
            fprintf(f, "%d %d %d", img->pixels[i][j].r, img->pixels[i][j].g, img->pixels[i][j].b);
            if (j < img->largura - 1) fprintf(f, "   ");
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("Imagem salva em '%s'.\n", caminho);
}

/* =============================================================
 * SELECAO DE REGIAO (RECORTE)
 * ============================================================= */

int perguntar_recorte(const Imagem *img, int *x1, int *y1, int *x2, int *y2) {
    char resp;
    printf("Aplicar em uma regiao especifica? (s/n): ");
    
    // Limpa espacos em branco ou quebras de linha pendentes
    scanf(" %c", &resp);

    if (resp != 's' && resp != 'S') {
        *x1 = 0;                *y1 = 0;
        *x2 = img->largura - 1; *y2 = img->altura - 1;
        return 0;
    }

    printf("Dimensoes: %d colunas x %d linhas\n", img->largura, img->altura);
    
    printf("Coluna inicial (0..%d): ", img->largura - 1); 
    while (scanf("%d", x1) != 1) { int c; while ((c = getchar()) != '\n' && c != EOF); printf("[ERRO] Digite um numero: "); }
    
    printf("Linha inicial  (0..%d): ", img->altura  - 1); 
    while (scanf("%d", y1) != 1) { int c; while ((c = getchar()) != '\n' && c != EOF); printf("[ERRO] Digite um numero: "); }
    
    printf("Coluna final   (%d..%d): ", *x1, img->largura - 1); 
    while (scanf("%d", x2) != 1) { int c; while ((c = getchar()) != '\n' && c != EOF); printf("[ERRO] Digite um numero: "); }
    
    printf("Linha final    (%d..%d): ", *y1, img->altura  - 1); 
    while (scanf("%d", y2) != 1) { int c; while ((c = getchar()) != '\n' && c != EOF); printf("[ERRO] Digite um numero: "); }

    *x1 = clamp(*x1, 0,   img->largura - 1);
    *y1 = clamp(*y1, 0,   img->altura  - 1);
    *x2 = clamp(*x2, *x1, img->largura - 1);
    *y2 = clamp(*y2, *y1, img->altura  - 1);
    return 1;
}

/* =============================================================
 * FILTROS DE COR
 * ============================================================= */

void filtro_negativo(Imagem *img, int x1, int y1, int x2, int y2) {
    int M = img->max_cor;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            img->pixels[i][j].r = M - img->pixels[i][j].r;
            img->pixels[i][j].g = M - img->pixels[i][j].g;
            img->pixels[i][j].b = M - img->pixels[i][j].b;
        }
}

void filtro_brilho(Imagem *img, int delta, int x1, int y1, int x2, int y2) {
    int M = img->max_cor;
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            img->pixels[i][j].r = (unsigned char)clamp(img->pixels[i][j].r + delta, 0, M);
            img->pixels[i][j].g = (unsigned char)clamp(img->pixels[i][j].g + delta, 0, M);
            img->pixels[i][j].b = (unsigned char)clamp(img->pixels[i][j].b + delta, 0, M);
        }
}

void filtro_cinza(Imagem *img, int x1, int y1, int x2, int y2) {
    for (int i = y1; i <= y2; i++)
        for (int j = x1; j <= x2; j++) {
            Pixel *p = &img->pixels[i][j];
            unsigned char c = (unsigned char)(0.299f * p->r + 0.587f * p->g + 0.114f * p->b);
            p->r = p->g = p->b = c;
        }
}

/* =============================================================
 * FILTROS POSICIONAIS
 * ============================================================= */

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

void filtro_rotacao180(Imagem *img, int x1, int y1, int x2, int y2) {
    filtro_flip_vertical(img, x1, y1, x2, y2);
    filtro_flip_horizontal(img, x1, y1, x2, y2);
}

/* =============================================================
 * CONVOLUCAO
 * ============================================================= */

void aplicar_convolucao(Imagem *img, float **nucleo, int n, int x1, int y1, int x2, int y2) {
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
 * MENU E PROGRAMA PRINCIPAL
 * ============================================================= */

void exibir_menu(void) {
    printf("\n========== MENU ==========\n");
    printf("--- Filtros de Cor ---\n");
    printf(" 1. Negativo\n");
    printf(" 2. Brilho\n");
    printf(" 3. Escala de Cinza\n");
    printf("--- Filtros Posicionais ---\n");
    printf(" 4. Flip Vertical\n");
    printf(" 5. Flip Horizontal\n");
    printf(" 6. Rotacao 180 graus\n");
    printf("--- Convolucao ---\n");
    printf(" 7. Convolucao com Nucleo Personalizado\n");
    printf("--- Arquivo ---\n");
    printf(" 8. Salvar imagem\n");
    printf(" 0. Sair\n");
    printf("==========================\n");
    printf("Opcao: ");
}

int main(void) {
    char caminho[512];

    printf("=== Manipulador de Imagens PPM P3 ===\n");
    printf("Arquivo PPM de entrada: ");

    if (!fgets(caminho, sizeof(caminho), stdin)) {
        fprintf(stderr, "Erro ao ler o nome do arquivo.\n");
        return 1;
    }
    caminho[strcspn(caminho, "\n")] = '\0';

    Imagem img = ler_ppm(caminho);
    printf("Imagem carregada: %d x %d pixels, max_cor=%d\n", img.largura, img.altura, img.max_cor);

    int opcao;
    do {
        exibir_menu();
        
        // TRATAMENTO DE ERRO: Se o usuário digitar letra em vez de número
        if (scanf("%d", &opcao) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // Limpa o buffer do teclado
            printf("\n[ERRO] Entrada invalida! Por favor, digite um numero.\n");
            opcao = -1; // Força a repetição do menu
            continue;
        }

        int x1, y1, x2, y2;

        switch (opcao) {
        case 1:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_negativo(&img, x1, y1, x2, y2);
            printf("Negativo aplicado.\n");
            break;

        case 2: {
            perguntar_recorte(&img, &x1, &y1, &x2, &y2); // Pergunta do recorte PRIMEIRO
            
            int delta;
            printf("Delta de brilho (negativo escurece, positivo ilumina): ");
            // Proteção contra letras
            while (scanf("%d", &delta) != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
                printf("[ERRO] Digite um numero valido para o delta: ");
            }
            
            filtro_brilho(&img, delta, x1, y1, x2, y2);
            printf("Brilho aplicado.\n");
            break;
        }

        case 3:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_cinza(&img, x1, y1, x2, y2);
            printf("Escala de cinza aplicada.\n");
            break;

        case 4:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_flip_vertical(&img, x1, y1, x2, y2);
            printf("Flip vertical aplicado.\n");
            break;

        case 5:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_flip_horizontal(&img, x1, y1, x2, y2);
            printf("Flip horizontal aplicado.\n");
            break;

        case 6:
            perguntar_recorte(&img, &x1, &y1, &x2, &y2);
            filtro_rotacao180(&img, x1, y1, x2, y2);
            printf("Rotacao 180 graus aplicada.\n");
            break;

        case 7: {
            perguntar_recorte(&img, &x1, &y1, &x2, &y2); // Pergunta do recorte PRIMEIRO
            
            int n;
            printf("Tamanho do nucleo NxN (minimo 3, deve ser impar): ");
            while (scanf("%d", &n) != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
                printf("[ERRO] Digite um numero valido: ");
            }
            if (n < 3) n = 3;
            if (n % 2 == 0) n++;

            float **nucleo = (float **)malloc(n * sizeof(float *));
            for (int i = 0; i < n; i++)
                nucleo[i] = (float *)malloc(n * sizeof(float));

            printf("Digite os %d valores do nucleo, linha por linha:\n", n * n);
            for (int i = 0; i < n; i++) {
                printf("  Linha %d: ", i);
                for (int j = 0; j < n; j++) {
                    while (scanf("%f", &nucleo[i][j]) != 1) {
                        int c; while ((c = getchar()) != '\n' && c != EOF);
                        printf("[ERRO] Valor invalido! Digite um numero para a posicao [%d][%d]: ", i, j);
                    }
                }
            }

            aplicar_convolucao(&img, nucleo, n, x1, y1, x2, y2);

            for (int i = 0; i < n; i++) free(nucleo[i]);
            free(nucleo);
            printf("Convolucao aplicada.\n");
            break;
        }

        case 8: {
            char saida[512];
            int c; while ((c = getchar()) != '\n' && c != EOF); // Limpa buffer antes de ler string
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
            if (opcao != -1) {
                printf("\n[ERRO] Opcao invalida. Escolha um numero de 0 a 8.\n");
            }
        }

    } while (opcao != 0);

    liberar_imagem(&img);
    return 0;
}