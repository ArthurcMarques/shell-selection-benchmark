#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static int saida_csv = 0;
static struct timespec inicio_programa;
static time_t horario_inicio;

static void formatar_horario(time_t horario, char *texto, size_t tamanho) {
    struct tm *local = localtime(&horario);
    if (!local || !strftime(texto, tamanho, "%Y-%m-%d %H:%M:%S %z", local))
        snprintf(texto, tamanho, "indisponivel");
}

static double segundos(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}
static void shell_pass(int *v, size_t n, size_t gap) {
    for (size_t i = gap; i < n; ++i) {
        int tmp = v[i];
        size_t j = i;
        while (j >= gap && v[j-gap] > tmp) {
            v[j] = v[j-gap];
            j -= gap;
        }
        v[j] = tmp;
    }
}
static int shell_sort(int *v, size_t n, const char *modo) {
    if (!strcmp(modo, "metades")) {
        for (size_t gap = n / 2; gap > 0; gap /= 2) shell_pass(v, n, gap);
    } else if (!strcmp(modo, "knuth")) {
        size_t gap = 1;
        while (gap <= (n - 1) / 3) gap = 3 * gap + 1;
        for (;;) {
            shell_pass(v, n, gap);
            if (gap == 1) break;
            gap /= 3;
        }
    } else if (!strcmp(modo, "ciura")) {
        const size_t gaps[] = {1,4,10,23,57,132,301,701,1750};
        size_t k = sizeof gaps / sizeof gaps[0];
        while (k > 0) {
            --k;
            if (gaps[k] < n) shell_pass(v, n, gaps[k]);
        }
        // Sequencia original de Ciura, sem extrapolacao para n > 1750.
    } else {
        fprintf(stderr, "Sequencia invalida: %s\n", modo);
        return 0;
    }
    return 1;
}
static int carregar(const char *arquivo, size_t n, int *v) {
    FILE *f = fopen(arquivo, "r");
    if (!f) { perror(arquivo); return 0; }
    for (size_t i = 0; i < n; ++i) {
        if (fscanf(f, "%d", &v[i]) != 1) {
            fprintf(stderr, "Arquivo incompleto ou invalido: %s, indice %zu\n", arquivo, i);
            fclose(f); return 0;
        }
    }
    int extra;
    if (fscanf(f, "%d", &extra) == 1) {
        fprintf(stderr, "Arquivo contem mais de %zu inteiros: %s\n", n, arquivo);
        fclose(f); return 0;
    }
    if (fclose(f)) { perror("fclose"); return 0; }
    return 1;
}
static int executar(const char *arquivo, size_t n, const char *tipo, const char *gap, double *tempo) {
    int *v = malloc(n * sizeof *v);
    if (!v) { perror("malloc"); return 1; }
    if (!carregar(arquivo, n, v)) { free(v); return 1; }
    if (saida_csv) fprintf(stderr, "Executando shell: %s, %zu elementos\n", tipo, n);
    else { printf("%-13s | %10zu | ", tipo, n); fflush(stdout); }
    struct timespec inicio, fim;
    if (clock_gettime(CLOCK_MONOTONIC, &inicio)) { perror("clock_gettime"); free(v); return 1; }
    if (!shell_sort(v, n, gap)) { free(v); return 2; }
    if (clock_gettime(CLOCK_MONOTONIC, &fim)) { perror("clock_gettime"); free(v); return 1; }
    for (size_t i = 1; i < n; ++i) {
        if (v[i-1] > v[i]) { fprintf(stderr, "Falha de ordenacao no indice %zu\n", i); free(v); return 1; }
    }
    *tempo = segundos(inicio, fim);
    if (saida_csv)
    printf("shell,%s,%zu,%s,%.9f,OK\n", tipo, n, gap, segundos(inicio, fim));
    else printf("%16.9f | OK\n", segundos(inicio, fim));
    free(v);
    return 0;
}

static int salvar_txt(const char *diretorio, const size_t *tamanhos, size_t quantidade,
                      const char **tipos, size_t casos, const double *tempos, const char *gap) {
    char pasta[4096], caminho[4096];
    int len = snprintf(pasta, sizeof pasta, "%s/../resultados", diretorio);
    if (len < 0 || (size_t)len >= sizeof pasta) { fprintf(stderr, "Caminho de resultados muito longo\n"); return 1; }
#ifdef _WIN32
    int criado = _mkdir(pasta);
#else
    int criado = mkdir(pasta, 0777);
#endif
    if (criado != 0 && errno != EEXIST) { perror(pasta); return 1; }
    len = snprintf(caminho, sizeof caminho, "%s/shell_%s.txt", pasta, gap);
    if (len < 0 || (size_t)len >= sizeof caminho) { fprintf(stderr, "Caminho de resultados muito longo\n"); return 1; }
    FILE *f = fopen(caminho, "w");
    if (!f) { perror(caminho); return 1; }
    fprintf(f, "Shell Sort - resultados\nEntradas: %s\n", diretorio);
    fprintf(f, "Sequencia de gaps: %s\n", gap);
    fprintf(f, "Tempo medido somente durante a ordenacao.\n\n");
    fprintf(f, "Teste   Entrada       |  Elementos |        Tempo (s) | Validacao\n");
    fprintf(f, "-------- --------------+------------+------------------+----------\n");
    for (size_t t = 0; t < quantidade; ++t)
        for (size_t c = 0; c < casos; ++c) {
            size_t i = t * casos + c;
            fprintf(f, "[%02zu/%02zu] %-13s | %10zu | %16.9f | OK\n",
                    i + 1, quantidade * casos, tipos[c], tamanhos[t], tempos[i]);
        }
    fprintf(f, "\nConcluido: %zu/%zu testes. Todos os vetores foram ordenados corretamente.\n", quantidade * casos, quantidade * casos);
    struct timespec fim_programa;
    if (clock_gettime(CLOCK_MONOTONIC, &fim_programa)) { perror("clock_gettime"); fclose(f); return 1; }
    char inicio_texto[64], fim_texto[64];
    formatar_horario(horario_inicio, inicio_texto, sizeof inicio_texto);
    formatar_horario(time(NULL), fim_texto, sizeof fim_texto);
    double duracao = segundos(inicio_programa, fim_programa);
    fprintf(f, "\nInicio: %s\nFim: %s\nTempo total do programa: %.6f segundos\n", inicio_texto, fim_texto, duracao);
    fprintf(f, "Inclui preparacao, leitura, ordenacao, validacao e escrita da tabela TXT; exclui o fechamento do relatorio.\n");
    int erro = ferror(f);
    if (fclose(f) != 0) erro = 1;
    if (erro) { fprintf(stderr, "Erro ao salvar relatorio: %s\n", caminho); return 1; }
    fprintf(saida_csv ? stderr : stdout, "\nInicio: %s\nFim: %s\nTempo total do programa: %.6f segundos\n", inicio_texto, fim_texto, duracao);
    fprintf(saida_csv ? stderr : stdout, "Relatorio TXT salvo em: %s\n", caminho);
    return 0;
}

/* Procura as entradas a partir do diretorio de execucao. */
static const char *localizar_testes(void) {
    const char *candidatos[] = {"testes", "../testes", "../../testes", "trabalho_ordenacao/testes"};
    for (size_t i = 0; i < sizeof candidatos / sizeof candidatos[0]; ++i) {
        char arquivo[4096];
        snprintf(arquivo, sizeof arquivo, "%s/aleatorio_10.txt", candidatos[i]);
        FILE *f = fopen(arquivo, "r");
        if (f) { fclose(f); return candidatos[i]; }
    }
    return "testes";
}

int main(int argc, char **argv) {
    horario_inicio = time(NULL);
    if (clock_gettime(CLOCK_MONOTONIC, &inicio_programa)) { perror("clock_gettime"); return 1; }
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--csv")) {
            saida_csv = 1;
            for (int j = i; j + 1 < argc; ++j) argv[j] = argv[j + 1];
            --argc; --i;
        }
    }
    if (argc > 3) {
        fprintf(stderr, "Uso: %s [--csv] [diretorio_testes] [metades|knuth|ciura]\n", argv[0]);
        return 2;
    }
    const char *diretorio = argc >= 2 ? argv[1] : localizar_testes();
    const char *gap = argc == 3 ? argv[2] : "metades";
    if (strcmp(gap, "metades") && strcmp(gap, "knuth") && strcmp(gap, "ciura")) {
        fprintf(stderr, "Sequencia invalida: %s\n", gap); return 2;
    }
    const size_t tamanhos[] = {10, 100, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 10000000};
    const char *tipos[] = {"aleatorio", "crescente", "decrescente"};
    /* Confirma todos os arquivos antes de iniciar a bateria. */
    for (size_t t = 0; t < sizeof tamanhos / sizeof tamanhos[0]; ++t) {
        for (size_t c = 0; c < sizeof tipos / sizeof tipos[0]; ++c) {
            char arquivo[4096];
            int len = snprintf(arquivo, sizeof arquivo, "%s/%s_%zu.txt", diretorio, tipos[c], tamanhos[t]);
            if (len < 0 || (size_t)len >= sizeof arquivo) { fprintf(stderr, "Caminho muito longo\n"); return 1; }
            FILE *f = fopen(arquivo, "r");
            if (!f) { perror(arquivo); fprintf(stderr, "Execute o gerador antes dos algoritmos.\n"); return 1; }
            fclose(f);
        }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    char inicio_texto[64];
    formatar_horario(horario_inicio, inicio_texto, sizeof inicio_texto);
    fprintf(saida_csv ? stderr : stdout, "Inicio: %s\n", inicio_texto);
    const size_t total = (sizeof tamanhos / sizeof tamanhos[0]) * (sizeof tipos / sizeof tipos[0]);
    double tempos[sizeof tamanhos / sizeof tamanhos[0] * (sizeof tipos / sizeof tipos[0])];
    if (saida_csv) puts("algoritmo,tipo,n,sequencia,segundos_ordenacao,validacao");
    else {
        printf("\nShell Sort - bateria de %zu testes\n", total);
        printf("Entradas: %s\n", diretorio);
        printf("Sequencia de gaps: %s\n", gap);
        puts("Tempo medido somente durante a ordenacao.\n");
        puts("Teste   Entrada       |  Elementos |        Tempo (s) | Validacao");
        puts("-------- --------------+------------+------------------+----------");
    }
    for (size_t t = 0; t < sizeof tamanhos / sizeof tamanhos[0]; ++t) {
        for (size_t c = 0; c < sizeof tipos / sizeof tipos[0]; ++c) {
            char arquivo[4096];
            snprintf(arquivo, sizeof arquivo, "%s/%s_%zu.txt", diretorio, tipos[c], tamanhos[t]);
            if (!saida_csv) printf("[%02zu/%02zu] ", t * (sizeof tipos / sizeof tipos[0]) + c + 1, total);
            int status = executar(arquivo, tamanhos[t], tipos[c], gap, &tempos[t * (sizeof tipos / sizeof tipos[0]) + c]);
            if (status) return status;
        }
    }
    if (!saida_csv) printf("\nConcluido: %zu/%zu testes. Todos os vetores foram ordenados corretamente.\n", total, total);
    return salvar_txt(diretorio, tamanhos, sizeof tamanhos / sizeof tamanhos[0],
                      tipos, sizeof tipos / sizeof tipos[0], tempos, gap);
}
