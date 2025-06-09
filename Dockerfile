FROM debian:bookworm-20250520-slim


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

# Copy the entire project into the container
COPY . .

# Create a build directory
RUN mkdir -p build && mkdir -p /etc/wind_breaker && chown -R 1001:1001 /etc/wind_breaker

# Set the working directory to the build directory
WORKDIR /app/build

# Remove any existing CMake cache
RUN rm -f CMakeCache.txt

# Run CMake to configure the project
RUN cmake ..

# Build the project
RUN cmake --build .

USER 1001:1001

# Specify the command to run the main executable by default
CMD ["stdbuf", "-oL", "-eL", "./main"]

