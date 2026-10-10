# Benchmark Shell Sort x Selection Sort com Docker Compose

## Requisitos
Docker Engine + plugin Docker Compose (`docker compose`).

## Preparacao
Execute os comandos dentro de `trabalho_ordenacao`:
```bash
cd trabalho_ordenacao
```
```bash
docker compose build
docker run --rm -v "${PWD}/testes:/dados/testes" trabalho-ordenacao:local gerador
```
O gerador gera 27 arquivos deterministas com 10, 100, 1000, 5000, 10000, 50000, 100000, 500000 e 1000000 numeros. O gerador e os dois algoritmos usam a mesma pasta `testes/` do repositorio, montada em `/dados/testes` por bind mount. O gerador pode escrever; os algoritmos montam a pasta somente para leitura. Os arquivos permanecem no host quando os conteineres sao removidos e podem ser versionados no Git.

## Executar e medir
Cada comando executa automaticamente os 27 testes: os nove tamanhos gerados, para entradas aleatorias, crescentes e decrescentes.

```bash
docker compose run --rm shell
docker compose run --rm selection
```
Shell usa gaps por metades por padrao. Para escolher outra sequencia com todos os casos:
```bash
docker compose run --rm shell /dados/testes knuth
docker compose run --rm shell /dados/testes ciura
```
A medicao e feita pelo proprio C com `clock_gettime(CLOCK_MONOTONIC)` antes e depois de cada ordenacao. Leitura e validacao ficam fora desse intervalo. Cada teste recarrega seu arquivo, sem modificar o volume. O programa verifica a disponibilidade dos 27 arquivos antes de iniciar e encerra com erro se um teste falhar.

Por padrao, a saida apresenta uma tabela com progresso, tipo de entrada, quantidade, tempo em segundos e validacao, seguida de um resumo. Para exportar, use `--csv`; esse modo inclui cabecalho CSV e uma linha por teste:
```text
algoritmo,tipo,n,sequencia,segundos_ordenacao,validacao
```
O progresso aparece no terminal (stderr). Para salvar os resultados no host:
```bash
docker compose run --rm -T shell --csv /dados/testes > resultados/shell.csv
docker compose run --rm -T selection --csv /dados/testes > resultados/selection.csv
```
Execute uma bateria por vez e repita as execucoes para comparar os tempos.

**Shell Sort** suporta `metades` (Shell), `knuth` e `ciura` (gaps finitos 1,4,10,23,57,132,301,701,1750, sem extrapolacao). Os tipos `crescente` e `decrescente` sao padroes de entrada; nao sao necessariamente os casos extremo teoricos para Shell Sort.

**Alerta:** Selection Sort em 500000 ou 1000000 elementos pode durar muito tempo devido a complexidade quadratica. A bateria completa inclui esses tamanhos.

## Integridade do volume
Os dois sorters montam `./testes` no mesmo caminho `/dados/testes`, em modo `:ro`. Depois que o gerador terminar, nao execute novamente o gerador durante os testes. Para comparar arquivos hash, opcionalmente:
```bash
docker compose run --rm --no-deps --entrypoint sha256sum shell /dados/testes/aleatorio_1000.txt
docker compose run --rm --no-deps --entrypoint sha256sum selection /dados/testes/aleatorio_1000.txt
```

## Arquivos
- `src/gerador.c`: gerador reproduzivel usado no projeto anterior
- `src/shell_sort.c`: Shell Sort e medicao
- `src/selection_sort.c`: Selection Sort e medicao
- `compose.yaml`: Shell e Selection compartilhando a pasta de entradas
- `testes/`: os 27 arquivos de entrada, disponiveis para Git e execucao local
- `resultados/`: CSVs salvos no host (nao no volume de entrada)

## Experimentos do seminario
O par do trabalho e Shell Sort x Selection Sort. Use os mesmos arquivos para ambos e registre a sequencia de gaps do Shell em cada comparacao. O CSV pode ser importado no Excel para construir graficos de linhas de tempo por quantidade de entradas. Registre tambem CPU, RAM e modelo do computador utilizado.

Selection Sort faz n(n-1)/2 comparacoes em qualquer entrada: melhor e pior caso sao Theta(n²). Para Shell com gaps por metades, o melhor caso e Theta(n log n) e o pior caso e Theta(n²); outras sequencias exigem analise propria. Entradas crescentes e decrescentes servem como cenarios experimentais, mas nao comprovam o pior caso teorico do Shell.

Os tempos, graficos e conclusoes devem vir de execucoes reais. Os programas imprimem `OK` somente depois de verificar a ordem do vetor.

## Executar fora do Docker
Dentro de `trabalho_ordenacao`, compile com GCC (Linux, WSL ou MSYS2/MinGW no Windows):
```bash
gcc -O2 -std=c11 -Wall -Wextra src/gerador.c -o gerador
gcc -O2 -std=c11 -Wall -Wextra src/shell_sort.c -o shell
gcc -O2 -std=c11 -Wall -Wextra src/selection_sort.c -o selection
./gerador
./shell
./selection
```
No PowerShell, use `-o gerador.exe`, `-o shell.exe` e `-o selection.exe`; execute com `.\gerador.exe`, `.\shell.exe` e `.\selection.exe`. Os algoritmos procuram `testes/`, `../testes`, `../../testes` e `trabalho_ordenacao/testes` a partir do diretorio atual (incluindo execucoes em `src/output`); um caminho diferente pode ser informado como primeiro argumento. Se os arquivos ja estiverem presentes, nao e preciso executar o gerador novamente.

A pasta `testes/` nao esta no `.gitignore` e pode ser enviada ao GitHub junto com os fontes. Os executaveis sao ignorados. As entradas ficam fora da imagem Docker e sao acessadas pela pasta montada.

No PowerShell, exporte CSV com `.\selection_sort.exe --csv > resultados.csv` (o caminho de saida e relativo ao diretorio atual).

## Relatorio TXT automatico
Ao concluir todos os testes com sucesso, cada programa salva a tabela e o resumo na pasta `resultados/`, ao lado de `testes/`: `selection.txt` e `shell_metades.txt` (ou `shell_knuth.txt` / `shell_ciura.txt`). Isso tambem acontece no modo `--csv`. Uma nova execucao substitui o relatorio correspondente. Se a bateria falhar, o relatorio final nao e atualizado.

No Docker, `./resultados` e montada em `/dados/resultados`, para manter os TXT no repositorio depois que o conteiner terminar.

## Ver os dois algoritmos no Compose
Execute `docker compose up --build` dentro de `trabalho_ordenacao`. O Compose inicia somente Shell e Selection juntos; os arquivos de entrada devem estar presentes em `testes/` antes da execucao. O terminal mostra as saidas com os prefixos dos servicos, como `shell-1` e `selection-1`. Cada programa exibe o caso atual antes de ordenar e o tempo quando ele termina. Os relatórios TXT ficam em `resultados/`. O Shell pode concluir enquanto Selection continua nos testes maiores.

Essa execucao simultanea serve para acompanhar o funcionamento. Para comparar tempos com menor interferencia, execute as baterias separadamente com `docker compose run --rm --no-deps shell` e `docker compose run --rm --no-deps selection`, depois de gerar as entradas.

## Horarios e duracao total
O terminal e o TXT registram inicio e fim com data, horario e deslocamento UTC, alem da duracao total medida por relogio monotonico desde a entrada em `main` ate a escrita da tabela TXT. Inclui preparacao, leitura, ordenacoes e validacoes; exclui o fechamento do relatorio. O tempo de cada ordenacao continua separado. No modo CSV, o resumo aparece em stderr para preservar as colunas. Localmente os horarios seguem o fuso do sistema; no Compose, `TZ=BRT3` configura UTC-03:00.

O gerador tambem localiza a raiz do projeto ao executar em `src/output`, salvando em `../../testes`. Para escolher outra pasta, passe seu caminho como primeiro argumento: `.\gerador.exe caminho_da_pasta`. O resumo informa a quantidade real de arquivos gerados.

## CSV automatico
Ao concluir a bateria, os programas salvam CSV e TXT em `resultados/`: `selection.csv` e `shell_metades.csv` (ou a sequencia escolhida). Funciona tambem no Compose, sem redirecionamento. `--csv` apenas muda a saida do terminal. Cada execucao substitui os arquivos correspondentes.

## Contadores nos programas C
Cada teste registra `comparacoes` (comparacoes entre valores durante a ordenacao) e `movimentacoes` (escritas em posicoes do vetor). Uma troca no Selection conta duas movimentacoes; copiar um valor para variavel temporaria nao conta. No Shell, cada deslocamento e a escrita final contam uma movimentacao, mesmo quando o valor e reescrito na mesma posicao. Leitura, validacao e controle dos lacos nao contam. Os contadores sao zerados a cada teste e usam `unsigned long long`. O tempo de ordenacao inclui o custo da contagem, portanto nao deve ser comparado diretamente com execucoes anteriores sem contadores.

Na bateria executada pelos programas C, CSV e TXT sao abertos no inicio (sobrescrevendo a execucao anterior) e cada resultado e gravado imediatamente, sem buffer. Uma interrupcao mantem os testes ja concluidos. O resumo final e escrito somente ao terminar a bateria; falhas de teste deixam uma indicacao de bateria interrompida no TXT.

## Tres variacoes na bateria C
Sem indicar gaps, o executavel `shell` executa metades, Knuth e Ciura em sequencia, recarregando as entradas em cada teste. Cada variacao tem CSV/TXT proprio em `resultados/`, gravado incrementalmente e sobrescrito na proxima execucao. O resumo de horarios e duracao refere-se a cada variacao. Execute `docker compose up -d --build shell` ou, localmente, `./shell testes`. Para apenas uma variacao, use `./shell testes knuth`. A saida `--csv` tem um unico cabecalho para todas as variacoes.
