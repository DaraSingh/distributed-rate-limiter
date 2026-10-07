# ---------- builder ----------
FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

ARG HIREDIS_VERSION=v1.2.0
ARG REDISPP_VERSION=1.3.10

RUN git clone --depth 1 --branch ${HIREDIS_VERSION} https://github.com/redis/hiredis.git \
 && cmake -S hiredis -B hiredis/build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build hiredis/build -j"$(nproc)" && cmake --install hiredis/build

RUN git clone --depth 1 --branch ${REDISPP_VERSION} https://github.com/sewenew/redis-plus-plus.git \
 && cmake -S redis-plus-plus -B redis-plus-plus/build \
      -DCMAKE_BUILD_TYPE=Release -DREDIS_PLUS_PLUS_CXX_STANDARD=17 \
 && cmake --build redis-plus-plus/build -j"$(nproc)" && cmake --install redis-plus-plus/build

WORKDIR /src
COPY CMakeLists.txt main.cpp ./
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"

# ---------- runtime ----------
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --no-create-home appuser
COPY --from=builder /usr/local/lib/libhiredis*.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libredis++*.so* /usr/local/lib/
RUN ldconfig
COPY --from=builder /src/build/demo /usr/local/bin/demo
WORKDIR /app
COPY script.lua /app/script.lua
COPY token_bucket.lua /app/token_bucket.lua
USER appuser
CMD ["demo"]