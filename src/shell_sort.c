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

/* Guarda os dados de um teste juntos. */
typedef struct {
    double tempo;
    unsigned long long comparacoes;
    unsigned long long movimentacoes;
} Resultado;

static int saida_csv = 0;
static unsigned long long comparacoes = 0;
static unsigned long long movimentacoes = 0;
static struct timespec inicio_programa;
static time_t horario_inicio;

static void imprimir_cabecalho(FILE *saida) {
    fprintf(saida, "Teste    Entrada       |  Elementos |        Tempo (s) |      Comparacoes |    Movimentacoes | Validacao\n--------- -------------+------------+------------------+------------------+------------------+----------\n");
}

static void imprimir_linha(FILE *saida, size_t teste, size_t total,
                          const char *tipo, size_t n, const Resultado *r) {
    fprintf(saida, "[%02zu/%02zu] %-13s | %10zu | %16.9f | %16llu | %16llu | OK\n",
            teste, total, tipo, n, r->tempo, r->comparacoes, r->movimentacoes);
}

static void imprimir_csv(FILE *saida, const char *tipo, size_t n,
                        const char *gap, const Resultado *r) {
    fprintf(saida, "shell,%s,%zu,%s,%.9f,%llu,%llu,OK\n",
            tipo, n, gap, r->tempo, r->comparacoes, r->movimentacoes);
}

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
        while (j >= gap) {
            comparacoes++;
            if (v[j-gap] <= tmp) break;
            v[j] = v[j-gap];
            movimentacoes++;
            j -= gap;
        }
        v[j] = tmp;
        movimentacoes++;
    }
}
static void shell_sort(int *v, size_t n, const char *modo) {
    if (!strcmp(modo, "metades")) {
        for (size_t gap = n / 2; gap > 0; gap /= 2) shell_pass(v, n, gap);
    } else if (!strcmp(modo, "knuth")) {
        size_t gap = 1;
        while (gap <= (n - 1) / 3) gap = 3 * gap + 1;
        for (; gap > 0; gap /= 3) shell_pass(v, n, gap);
    } else if (!strcmp(modo, "ciura")) {
        const size_t gaps[] = {1,4,10,23,57,132,301,701,1750};
        size_t k = sizeof gaps / sizeof gaps[0];
        while (k > 0) {
            --k;
            if (gaps[k] < n) shell_pass(v, n, gaps[k]);
        }
        // Sequencia original de Ciura, sem extrapolacao para n > 1750.
    }
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
static int executar(const char *arquivo, size_t n, const char *tipo, const char *gap, Resultado *resultado) {
    if (n == 0 || n > SIZE_MAX / sizeof(int)) {
        fprintf(stderr, "Quantidade invalida\n");
        return 1;
    }
    int *v = malloc(n * sizeof *v);
    if (!v) { perror("malloc"); return 1; }
    if (!carregar(arquivo, n, v)) { free(v); return 1; }

    comparacoes = 0;
    movimentacoes = 0;
    struct timespec inicio, fim;
    if (clock_gettime(CLOCK_MONOTONIC, &inicio)) { perror("clock_gettime"); free(v); return 1; }
    shell_sort(v, n, gap);
    if (clock_gettime(CLOCK_MONOTONIC, &fim)) { perror("clock_gettime"); free(v); return 1; }
    for (size_t i = 1; i < n; ++i) {
        if (v[i-1] > v[i]) { fprintf(stderr, "Falha de ordenacao no indice %zu\n", i); free(v); return 1; }
    }
    resultado->tempo = segundos(inicio, fim);
    resultado->comparacoes = comparacoes;
    resultado->movimentacoes = movimentacoes;
    if (saida_csv) imprimir_csv(stdout, tipo, n, gap, resultado);

    free(v);
    return 0;
}

static int iniciar_relatorios(const char *diretorio, FILE **arquivo_csv,
                              FILE **arquivo_txt, const char *gap) {
    char pasta[4096], caminho[4096];
    int len = snprintf(pasta, sizeof pasta, "%s/../resultados", diretorio);
    if (len < 0 || (size_t)len >= sizeof pasta) { fprintf(stderr, "Caminho de resultados muito longo\n"); return 1; }
#ifdef _WIN32
    int criado = _mkdir(pasta);
#else
    int criado = mkdir(pasta, 0777);
#endif
    if (criado != 0 && errno != EEXIST) { perror(pasta); return 1; }
    char caminho_csv[4096];
    len = snprintf(caminho_csv, sizeof caminho_csv, "%s/shell_%s.csv", pasta, gap);
    if (len < 0 || (size_t)len >= sizeof caminho_csv) { fprintf(stderr, "Caminho CSV muito longo\n"); return 1; }
    FILE *csv = fopen(caminho_csv, "w");
    if (!csv) { perror(caminho_csv); return 1; }
    setvbuf(csv, NULL, _IONBF, 0);
    fprintf(csv, "algoritmo,tipo,n,sequencia,segundos_ordenacao,comparacoes,movimentacoes,validacao\n");
    len = snprintf(caminho, sizeof caminho, "%s/shell_%s.txt", pasta, gap);
    if (len < 0 || (size_t)len >= sizeof caminho) { fprintf(stderr, "Caminho de resultados muito longo\n"); fclose(csv); return 1; }
    FILE *f = fopen(caminho, "w");
    if (!f) { perror(caminho); fclose(csv); return 1; }
    setvbuf(f, NULL, _IONBF, 0);
    fprintf(f, "Shell Sort - resultados\nEntradas: %s\n", diretorio);
    fprintf(f, "Sequencia de gaps: %s\n", gap);
    fprintf(f, "Tempo medido somente durante a ordenacao.\n\n");
    imprimir_cabecalho(f);
    *arquivo_csv = csv;
    *arquivo_txt = f;
    if (ferror(csv) || ferror(f)) {
        fprintf(stderr, "Erro ao iniciar relatorios\n");
        fclose(csv); fclose(f); return 1;
    }
    fprintf(saida_csv ? stderr : stdout, "Relatorios: %s e %s\n", caminho_csv, caminho);
    return 0;
}

static int finalizar_relatorios(FILE *csv, FILE *f, size_t total) {
    fprintf(f, "\nConcluido: %zu/%zu testes. Todos os vetores foram ordenados corretamente.\n", total, total);
    struct timespec fim_programa;
    if (clock_gettime(CLOCK_MONOTONIC, &fim_programa)) { perror("clock_gettime"); fclose(csv); fclose(f); return 1; }
    char inicio_texto[64], fim_texto[64];
    formatar_horario(horario_inicio, inicio_texto, sizeof inicio_texto);
    formatar_horario(time(NULL), fim_texto, sizeof fim_texto);
    double duracao = segundos(inicio_programa, fim_programa);
    fprintf(f, "\nInicio: %s\nFim: %s\nTempo total do programa: %.6f segundos\n", inicio_texto, fim_texto, duracao);
    fprintf(f, "Inclui preparacao, leitura, ordenacao, validacao e escrita do CSV e da tabela TXT; exclui o fechamento do relatorio.\n");
    int erro = ferror(f) || ferror(csv);
    if (fclose(csv) != 0) erro = 1;
    if (fclose(f) != 0) erro = 1;
    if (erro) { fprintf(stderr, "Erro ao salvar relatorios\n"); return 1; }
    fprintf(saida_csv ? stderr : stdout, "\nInicio: %s\nFim: %s\nTempo total do programa: %.6f segundos\n", inicio_texto, fim_texto, duracao);
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
    const char *variacoes[] = {"metades", "knuth", "ciura"};
    int quantidade_variacoes = 3;
    if (argc == 3) {
        if (strcmp(argv[2], "metades") && strcmp(argv[2], "knuth") && strcmp(argv[2], "ciura")) {
            fprintf(stderr, "Sequencia invalida: %s\n", argv[2]); return 2;
        }
        variacoes[0] = argv[2];
        quantidade_variacoes = 1;
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
    if (saida_csv) puts("algoritmo,tipo,n,sequencia,segundos_ordenacao,comparacoes,movimentacoes,validacao");
    for (int variacao = 0; variacao < quantidade_variacoes; variacao++) {
        const char *gap = variacoes[variacao];
        horario_inicio = time(NULL);
        if (clock_gettime(CLOCK_MONOTONIC, &inicio_programa)) { perror("clock_gettime"); return 1; }
        char inicio_texto[64];
        formatar_horario(horario_inicio, inicio_texto, sizeof inicio_texto);
        fprintf(saida_csv ? stderr : stdout, "Inicio: %s\n", inicio_texto);
        const size_t total = (sizeof tamanhos / sizeof tamanhos[0]) * (sizeof tipos / sizeof tipos[0]);
        Resultado resultado;
        FILE *csv, *txt;
        if (iniciar_relatorios(diretorio, &csv, &txt, gap)) return 1;
        if (!saida_csv) {
            printf("\nShell Sort - bateria de %zu testes\n", total);
            printf("Entradas: %s\n", diretorio);
            printf("Sequencia de gaps: %s\n", gap);
            puts("Tempo medido somente durante a ordenacao.\n");
            imprimir_cabecalho(stdout);
        }
        for (size_t t = 0; t < sizeof tamanhos / sizeof tamanhos[0]; ++t) {
            for (size_t c = 0; c < sizeof tipos / sizeof tipos[0]; ++c) {
                char arquivo[4096];
                snprintf(arquivo, sizeof arquivo, "%s/%s_%zu.txt", diretorio, tipos[c], tamanhos[t]);
                size_t indice = t * (sizeof tipos / sizeof tipos[0]) + c;
                if (saida_csv) {
                    fprintf(stderr, "Executando [%02zu/%02zu]: %s, %zu elementos\n", indice + 1, total, tipos[c], tamanhos[t]);
                    fflush(stderr);
                }
                int status = executar(arquivo, tamanhos[t], tipos[c], gap, &resultado);
                if (status) {
                    fprintf(txt, "\nBateria interrompida apos %zu testes concluidos; teste atual falhou.\n", indice);
                    fclose(csv); fclose(txt); return status;
                }
                imprimir_csv(csv, tipos[c], tamanhos[t], gap, &resultado);
                imprimir_linha(txt, indice + 1, total, tipos[c], tamanhos[t], &resultado);
                if (ferror(csv) || ferror(txt)) {
                    fprintf(stderr, "Erro ao gravar resultado\n");
                    fclose(csv); fclose(txt); return 1;
                }
                if (!saida_csv) imprimir_linha(stdout, indice + 1, total, tipos[c],
                                               tamanhos[t], &resultado);
            }
        }
        if (!saida_csv) printf("\nConcluido: %zu/%zu testes. Todos os vetores foram ordenados corretamente.\n", total, total);
        if (finalizar_relatorios(csv, txt, total)) return 1;
    }
    return 0;
}
