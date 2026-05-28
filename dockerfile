FROM gcc:12.2 AS builder
WORKDIR /app
COPY CMakeLists.txt .
COPY src/ src/
COPY static/ static/
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -- -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/rapidserve /usr/local/bin/rapidserve
COPY static/ /static
EXPOSE 8080
CMD ["rapidserve", "--static", "/static"]
