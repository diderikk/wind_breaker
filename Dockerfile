FROM debian:bookworm-20250811-slim AS builder

RUN apt update && apt upgrade -y && apt install -y --no-install-recommends \
    curl \
    libcurl4-openssl-dev \
    ca-certificates \
    gcc \
    make \
    cmake \
    libssl-dev \
    zlib1g-dev \
    sqlite3 \
    libsqlite3-dev && \
    apt clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN mkdir -p build 

WORKDIR /app/build

RUN rm -f CMakeCache.txt

RUN cmake ..

RUN cmake --build . --target main



FROM debian:bookworm-20250811-slim 

# Does not have static libraries...
RUN apt update && apt upgrade -y && apt install -y --no-install-recommends \
    libssl-dev \
    sqlite3
    
WORKDIR /app

COPY --from=builder /app/build .

RUN mkdir -p /etc/wind_breaker && chown -R 1001:1001 /etc/wind_breaker

USER 1001:1001

# Flush after every new line (for debugging purposes)
CMD ["stdbuf", "-oL", "-eL", "./main"]

