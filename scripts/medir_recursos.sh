#!/bin/sh
# Um processo por teste, sequencialmente, no mesmo container.
set -eu
export LC_ALL=C
algoritmos='shell selection'
tamanhos=''
tipos='aleatorio crescente decrescente'
gaps='metades knuth ciura'
repeticoes=1
entradas=/dados/testes
saida=/dados/resultados/recursos.csv
uso() {
  echo 'Uso: medir-recursos [--algoritmos "shell selection"] [--tamanhos "10 100"] [--tipos "aleatorio crescente decrescente"] [--gaps "metades knuth ciura"] [--repeticoes N] [--entradas DIR] [--saida CSV]' >&2
}
while [ "$#" -gt 0 ]; do
  case "$1" in
    --help) uso; exit 0 ;;
    --algoritmos|--tamanhos|--tipos|--gaps|--repeticoes|--entradas|--saida)
      [ "$#" -ge 2 ] || { uso; exit 2; }
      case "$1" in
        --algoritmos) algoritmos=$2 ;; --tamanhos) tamanhos=$2 ;;
        --tipos) tipos=$2 ;; --gaps) gaps=$2 ;; --repeticoes) repeticoes=$2 ;;
        --entradas) entradas=$2 ;; --saida) saida=$2 ;;
      esac
      shift 2 ;;
    *) uso; exit 2 ;;
  esac
done
[ -n "$algoritmos" ] && [ -n "$tipos" ] && [ -n "$gaps" ] || exit 2
for a in $algoritmos; do case "$a" in shell|selection) ;; *) uso; exit 2 ;; esac; done
for t in $tipos; do case "$t" in aleatorio|crescente|decrescente) ;; *) uso; exit 2 ;; esac; done
for g in $gaps; do case "$g" in metades|knuth|ciura) ;; *) uso; exit 2 ;; esac; done
for n in $tamanhos; do
  case "$n" in ''|*[!0-9]*) uso; exit 2 ;; esac
  [ "$n" -gt 0 ] || exit 2
done
[ -d "$entradas" ] || { echo "Pasta inexistente: $entradas" >&2; exit 2; }
case "$repeticoes" in ''|*[!0-9]*) uso; exit 2 ;; esac
[ "$repeticoes" -ge 1 ] && [ "$repeticoes" -le 1000000 ] || exit 2
/usr/bin/time --version | grep -q 'GNU' || { echo 'GNU Time necessario.' >&2; exit 2; }
mkdir -p "$(dirname "$saida")"
cabecalho='algoritmo,tipo,n,sequencia,repeticao,segundos_ordenacao,segundos_processo,cpu_usuario_s,cpu_sistema_s,max_rss_kib,codigo_saida,validacao'
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
: > "$tmp/lista"
for arquivo in "$entradas"/*.txt; do
  [ -f "$arquivo" ] || continue
  nome=${arquivo##*/}
  t=${nome%_*}
  n=${nome##*_}
  n=${n%.txt}
  case "$t" in aleatorio|crescente|decrescente) ;; *) echo "Ignorando nome fora do padrao: $nome" >&2; continue ;; esac
  case "$n" in ''|*[!0-9]*|0*) echo "Ignorando quantidade invalida: $nome" >&2; continue ;; esac
  case " $tipos " in *" $t "*) ;; *) continue ;; esac
  if [ -n "$tamanhos" ]; then
    case " $tamanhos " in *" $n "*) ;; *) continue ;; esac
  fi
  printf '%s|%s|%s\n' "$n" "$t" "$nome" >> "$tmp/lista"
done
sort -t '|' -k1,1n -k2,2 "$tmp/lista" > "$tmp/ordenados"
quantidade=$(wc -l < "$tmp/ordenados")
[ "$quantidade" -gt 0 ] || { echo 'Nenhum teste selecionado.' >&2; exit 2; }
total_variacao=$((quantidade * repeticoes))
printf '%s\n' "$cabecalho" > "$saida"

txt="${saida%.csv}.txt"
[ "$txt" != "$saida" ] || txt="$saida.txt"
cabecalho_txt() {
  printf 'Teste    Entrada       |   Elementos | Rep | Algoritmo | Gaps    |        Tempo (s) | Total (s) | Usuario (s) | Sistema (s) | RSS (KiB) | Saida | Validacao\n'
  printf '%s\n' '-------- --------------+-------------+-----+-----------+---------+------------------+-----------+-------------+-------------+-----------+-------+----------'
}
inicio=$(date '+%Y-%m-%d %H:%M:%S %z')
inicio_segundos=$(date +%s)
printf '\nResultados de ordenacao\nEntradas: %s\nInicio: %s\nTempo (s): somente a ordenacao; demais recursos: processo inteiro.\n\n' "$entradas" "$inicio" > "$txt"
cabecalho_txt >> "$txt"
falhas=0
concluidos=0
for a in $algoritmos; do
  sequencias=na
  [ "$a" != shell ] || sequencias=$gaps
  for g in $sequencias; do
    nome_relatorio=$a
    [ "$a" != shell ] || nome_relatorio="shell_$g"
    pasta_variacoes="$(dirname "$saida")/variacoes"
    mkdir -p "$pasta_variacoes"
    csv_variacao="$pasta_variacoes/$nome_relatorio.csv"
    txt_variacao="$pasta_variacoes/$nome_relatorio.txt"
    [ "$csv_variacao" != "$saida" ] && [ "$txt_variacao" != "$txt" ] || { echo 'Use um nome de CSV consolidado diferente dos relatorios individuais.' >&2; exit 2; }
    printf '%s\n' "$cabecalho" > "$csv_variacao"
    inicio_variacao=$(date '+%Y-%m-%d %H:%M:%S %z')
    inicio_variacao_segundos=$(date +%s)
    titulo='Selection Sort'
    [ "$a" != shell ] || titulo='Shell Sort'
    printf '\n%s - resultados\nEntradas: %s\nSequencia de gaps: %s\nInicio: %s\nTempo (s): somente a ordenacao; demais recursos: processo inteiro.\n\n' "$titulo" "$entradas" "$g" "$inicio_variacao" > "$txt_variacao"
    cat "$txt_variacao"
    cabecalho_txt | tee -a "$txt_variacao"
    testes_variacao=0
    while IFS='|' read -r n t nome; do
      arquivo="$entradas/$nome"
      r=1
      while [ "$r" -le "$repeticoes" ]; do
        set -- "$a" --csv "$arquivo" "$n" "$t"
        [ "$a" != shell ] || set -- "$@" "$g"
        rc=0
        /usr/bin/time -q -f '%e,%U,%S,%M' -o "$tmp/time" "$@" > "$tmp/out" 2> "$tmp/err" || rc=$?
        cat "$tmp/err" >&2
        tempo=''
        validacao=FALHA
        if [ "$rc" -eq 0 ]; then
          tempo=$(awk -F, -v a="$a" -v t="$t" -v n="$n" -v g="$g" 'NR==2 && NF==6 && $1==a && $2==t && $3==n && $4==g && $5 ~ /^[0-9]+\.[0-9]+$/ && $6=="OK" {print $5}' "$tmp/out")
          if [ -n "$tempo" ] && [ "$(wc -l < "$tmp/out")" -eq 2 ]; then validacao=OK; else rc=125; fi
        fi
        metricas=$(cat "$tmp/time")
        if ! printf '%s\n' "$metricas" | awk -F, 'NF==4 && $1 ~ /^[0-9]+([.][0-9]+)?$/ && $2 ~ /^[0-9]+([.][0-9]+)?$/ && $3 ~ /^[0-9]+([.][0-9]+)?$/ && $4 ~ /^[0-9]+$/ {ok=1} END {exit !ok}'; then
          metricas=',,,'
          tempo=''
          validacao=FALHA
          [ "$rc" -ne 0 ] || rc=125
        fi
        # Cada teste e gravado imediatamente nos relatorios desta execucao.
        printf '%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$a" "$t" "$n" "$g" "$r" "$tempo" "$metricas" "$rc" "$validacao" >> "$saida"
        printf '%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$a" "$t" "$n" "$g" "$r" "$tempo" "$metricas" "$rc" "$validacao" >> "$csv_variacao"
        indice=$((testes_variacao + 1))
        # Separar as metricas para alinhar cada coluna do TXT.
        IFS=',' read -r total usuario sistema rss < "$tmp/time" || true
        tempo_txt=$tempo
        [ -n "$tempo_txt" ] || tempo_txt='-'
        linha=$(printf '[%02d/%02d] %-13s | %11s | %3s | %-9s | %-7s | %16s | %9s | %11s | %11s | %9s | %5s | %s' "$indice" "$total_variacao" "$t" "$n" "$r" "$a" "$g" "$tempo_txt" "${total:--}" "${usuario:--}" "${sistema:--}" "${rss:--}" "$rc" "$validacao")
        printf '%s\n' "$linha" >> "$txt"
        printf '%s\n' "$linha" | tee -a "$txt_variacao"
        testes_variacao=$((testes_variacao + 1))
        concluidos=$((concluidos + 1))
        [ "$rc" -eq 0 ] || falhas=1
        r=$((r + 1))
      done
    done < "$tmp/ordenados"
    printf '\nConcluido: %s/%s testes.\nInicio: %s\nFim: %s\nTempo total da bateria: %s segundos\n' "$testes_variacao" "$total_variacao" "$inicio_variacao" "$(date '+%Y-%m-%d %H:%M:%S %z')" "$(( $(date +%s) - inicio_variacao_segundos ))" | tee -a "$txt_variacao"
    echo "Relatorios: $csv_variacao e $txt_variacao" >&2
  done
done
printf '\nTestes concluidos: %s\nInicio: %s\nFim: %s\nTempo total da bateria: %s segundos\n' "$concluidos" "$inicio" "$(date '+%Y-%m-%d %H:%M:%S %z')" "$(( $(date +%s) - inicio_segundos ))" >> "$txt"
[ "$concluidos" -gt 0 ] || { echo 'Nenhum teste selecionado.' >&2; exit 2; }
echo "CSV salvo em: $saida; TXT salvo em: $txt" >&2
exit "$falhas"
