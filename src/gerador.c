#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static uint32_t estado = 20261008u;

static uint32_t aleatorio32(void) {
    estado ^= estado << 13;
    estado ^= estado >> 17;
    estado ^= estado << 5;
    return estado;
}

static int salvar(const char *diretorio, const char *tipo, size_t n, const int *v) {
    char caminho[4096];
    int len = snprintf(caminho, sizeof caminho, "%s/%s_%zu.txt", diretorio, tipo, n);
    if (len < 0 || (size_t)len >= sizeof caminho) { fprintf(stderr, "Caminho muito longo\n"); return 0; }
    FILE *f = fopen(caminho, "w");
    if (!f) { perror(caminho); return 0; }
    for (size_t i = 0; i < n; ++i) {
        if (fprintf(f, "%d\n", v[i]) < 0) {
            perror("Erro de escrita"); fclose(f); return 0;
        }
    }
    if (fclose(f) != 0) { perror("Erro ao fechar arquivo"); return 0; }
    printf("Gerado: %s\n", caminho);
    return 1;
}

static const char *localizar_destino(void) {
    const char *raizes[] = {".", "..", "../..", "trabalho_ordenacao"};
    const char *pastas[] = {"testes", "../testes", "../../testes", "trabalho_ordenacao/testes"};
    for (size_t i = 0; i < sizeof raizes / sizeof raizes[0]; ++i) {
        char fonte[4096];
        snprintf(fonte, sizeof fonte, "%s/src/gerador.c", raizes[i]);
        FILE *f = fopen(fonte, "r");
        if (f) { fclose(f); return pastas[i]; }
    }
    /* No Docker, o diretorio de trabalho e /dados. */
    return "testes";
}

int main(int argc, char **argv) {
    if (argc > 2) { fprintf(stderr, "Uso: %s [diretorio_testes]\n", argv[0]); return 2; }
    const char *diretorio = argc == 2 ? argv[1] : localizar_destino();
    printf("Pasta de destino: %s\n", diretorio);
    const size_t tamanhos[] = {10, 100, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 10000000};
    const size_t total = sizeof tamanhos / sizeof tamanhos[0];

#ifdef _WIN32
    if (_mkdir(diretorio) != 0 && errno != EEXIST) { perror(diretorio); return 1; }
#else
    if (mkdir(diretorio, 0777) != 0 && errno != EEXIST) { perror(diretorio); return 1; }
#endif

    for (size_t t = 0; t < total; ++t) {
        size_t n = tamanhos[t];
        int *v = malloc(n * sizeof *v);
        if (!v) { fprintf(stderr, "Memoria insuficiente para %zu elementos\n", n); return 1; }

        for (size_t i = 0; i < n; ++i) v[i] = (int)(i + 1);
        if (!salvar(diretorio, "crescente", n, v)) { free(v); return 1; }

        for (size_t i = 0; i < n; ++i) v[i] = (int)(n - i);
        if (!salvar(diretorio, "decrescente", n, v)) { free(v); return 1; }

        for (size_t i = 0; i < n; ++i) v[i] = (int)(i + 1);
        for (size_t i = n - 1; i > 0; --i) {
            size_t j = (size_t)(aleatorio32() % (uint32_t)(i + 1));
            int aux = v[i]; v[i] = v[j]; v[j] = aux;
        }
        if (!salvar(diretorio, "aleatorio", n, v)) { free(v); return 1; }
        free(v);
    }
    printf("Concluido: %zu arquivos gerados em %s.\n", total * 3, diretorio);
    return 0;
}
