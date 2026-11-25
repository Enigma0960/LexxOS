FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential gcc-arm-linux-gnueabi bison flex libssl-dev \
    python3 python-is-python3 device-tree-compiler bc \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
