# Editor de Imagens PPM (P3) 🖼️

Este repositório contém o desenvolvimento de um sistema em C capaz de ler, manipular e salvar imagens no formato estrito PPM (variante P3). O projeto foi desenvolvido como trabalho prático (Laboratório 2) e foca na manipulação direta de matrizes de pixels alocadas dinamicamente.

## ✨ Funcionalidades Implementadas

O sistema atua através de um menu interativo no terminal e permite aplicar transformações na imagem inteira ou em um recorte/seleção específico (informando coordenadas X e Y).

### Filtros de Cor

* **Negativo:** Inverte os canais RGB da imagem.
* **Brilho:** Adiciona ou subtrai intensidade luminosa dos pixels usando fator `clamp`.
* **Escala de Cinza:** Converte a imagem utilizando ponderação perceptual (ITU-R BT.601).

### Filtros Posicionais

* **Flip Vertical:** Espelha a imagem de cima para baixo.
* **Flip Horizontal:** Espelha a imagem da esquerda para a direita.
* **Rotação 180°:** Rotaciona a imagem invertendo linhas e colunas.

### Operações Avançadas

* **Convolução NxN:** Permite a aplicação de filtros avançados (como Blur, Sharpen, Edge Detection) através de uma matriz de convolução com tamanho ímpar configurável (mínimo 3x3) inserida pelo próprio usuário. Trata bordas utilizando clamp padding.

## 🚀 Como Compilar e Executar

Certifique-se de ter o compilador GCC instalado no seu ambiente.

1. Clone o repositório ou baixe os arquivos.
2. Navegue até a pasta do código fonte:
```bash
   cd terminal
```
3. Compile o arquivo vinculando a biblioteca matemática (`-lm`):
```bash
   gcc -o ppm_filtros ppm_filtros.c -lm
```
4. Execute o programa:
```bash
   # No Linux/macOS:
   ./ppm_filtros

   # No Windows:
   .\ppm_filtros.exe
```

## 📂 Estrutura do Repositório

* `/terminal`: Contém o código fonte principal (`ppm_filtros.c`) e as instruções rápidas.
* `leiame.txt`: Documentação detalhada sobre o funcionamento interno matemático de cada filtro e manual de uso completo.

---

**Autor:** Deivid Da Silva Trindade