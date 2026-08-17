FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --no-install-recommends --yes \
        ca-certificates \
        cmake \
        dpkg-dev \
        file \
        g++ \
        libsystemd-dev \
        ninja-build \
        pkg-config \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

ENTRYPOINT ["/workspace/scripts/container-package.sh"]
