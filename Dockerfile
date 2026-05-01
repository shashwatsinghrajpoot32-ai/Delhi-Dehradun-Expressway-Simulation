FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j

FROM ubuntu:24.04 AS run

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Runtime needs: server binary + input file + frontend assets
COPY --from=build /app/build/expressway_server ./expressway_server
COPY --from=build /app/expressway.txt ./expressway.txt
COPY --from=build /app/frontend ./frontend

ENV PORT=8080
EXPOSE 8080

CMD ["./expressway_server"]
