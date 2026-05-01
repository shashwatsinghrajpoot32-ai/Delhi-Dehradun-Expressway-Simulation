FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    g++ \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy everything needed to build + run.
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j

ENV PORT=8080
EXPOSE 8080

CMD ["./build/expressway_server"]
