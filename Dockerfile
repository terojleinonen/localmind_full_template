# syntax=docker/dockerfile:1

# ---- build ------------------------------------------------------------------
FROM debian:bookworm-slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake make libsqlite3-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)" \
 && ctest --test-dir build --output-on-failure

# ---- runtime ----------------------------------------------------------------
FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libsqlite3-0 curl \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --uid 10001 --home /app localmind
WORKDIR /app
COPY --from=build /src/build/localmind_server /usr/local/bin/localmind_server
COPY web ./web
COPY data/docs ./samples
RUN mkdir -p /app/data && chown localmind /app/data
USER localmind

ENV LOCALMIND_HOST=0.0.0.0 \
    LOCALMIND_PORT=8080 \
    LOCALMIND_DB=/app/data/localmind.db \
    LOCALMIND_WEB_DIR=/app/web
EXPOSE 8080
VOLUME ["/app/data"]
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s \
  CMD curl -fsS http://127.0.0.1:8080/api/health || exit 1
ENTRYPOINT ["localmind_server"]
