FROM alpine:3.24 AS build
RUN apk add --no-cache gcc musl-dev
WORKDIR /src
COPY src/ .
RUN gcc -O2 -std=c11 -Wall -Wextra shell_sort.c -o /shell && \
    gcc -O2 -std=c11 -Wall -Wextra selection_sort.c -o /selection && \
    gcc -O2 -std=c11 -Wall -Wextra gerador.c -o /gerador
FROM alpine:3.24
COPY --from=build /shell /usr/local/bin/shell
COPY --from=build /selection /usr/local/bin/selection
COPY --from=build /gerador /usr/local/bin/gerador
WORKDIR /dados
