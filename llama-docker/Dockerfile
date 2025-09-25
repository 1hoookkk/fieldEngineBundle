FROM ubuntu:22.04 AS cpu

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake git ca-certificates \
       bash coreutils \
    && rm -rf /var/lib/apt/lists/*

# Build llama.cpp (CPU) with server
WORKDIR /opt
RUN git clone --depth 1 https://github.com/ggerganov/llama.cpp.git \
    && cmake -S llama.cpp -B build \
         -DLLAMA_BUILD_SERVER=ON \
         -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j \
    && install -m 0755 build/bin/* /usr/local/bin/

# Model mount point
RUN mkdir -p /models
VOLUME ["/models"]

# Simple entrypoint to launch the server
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

EXPOSE 8080

ENV SERVER_PORT=8080 \
    CTX_SIZE=4096 \
    THREADS=0 \
    NGL=0 \
    MODEL_PATH="" \
    SERVER_EXTRA_ARGS=""

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]


# -------- CUDA/cuBLAS build target --------
FROM nvidia/cuda:12.4.1-devel-ubuntu22.04 AS cuda

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake git ca-certificates \
       bash coreutils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone --depth 1 https://github.com/ggerganov/llama.cpp.git \
    && cmake -S llama.cpp -B build \
         -DLLAMA_BUILD_SERVER=ON \
         -DLLAMA_CUBLAS=ON \
         -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j \
    && install -m 0755 build/bin/* /usr/local/bin/

RUN mkdir -p /models
VOLUME ["/models"]

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

EXPOSE 8080

ENV SERVER_PORT=8080 \
    CTX_SIZE=4096 \
    THREADS=0 \
    NGL=35 \
    MODEL_PATH="" \
    SERVER_EXTRA_ARGS=""

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

