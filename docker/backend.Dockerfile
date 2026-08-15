FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git libsqlite3-dev \
    libboost-all-dev libssl-dev libasio-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app/backend
COPY backend/ .

RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc)

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y \
    libsqlite3-0 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/backend/build/academia_server .

ENV ACADEMIA_PORT=8080
ENV ACADEMIA_DB_PATH=/app/data/academia.db
ENV ACADEMIA_LOG_FILE=/app/logs/server.log

RUN mkdir -p /app/data /app/logs

EXPOSE 8080

CMD ["./academia_server"]
