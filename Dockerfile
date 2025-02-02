# Use an official Alpine Linux image
FROM alpine:latest

# Install necessary dependencies
RUN apk update && apk add --no-cache \
    cmake \
    gcc \
    make \
    openssl-dev \
    zlib-dev \
    musl-dev

# Set the working directory
WORKDIR /app

# Copy the entire project into the container
COPY . .

# Create a build directory
RUN mkdir -p build

# Set the working directory to the build directory
WORKDIR /app/build

# Remove any existing CMake cache
RUN rm -f CMakeCache.txt

# Run CMake to configure the project
RUN cmake ..

# Build the project
RUN cmake --build .

# Specify the command to run the main executable by default
CMD ["./main"]
