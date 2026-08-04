FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --no-install-recommends --yes \
        ca-certificates \
        clang-format \
        clang-tidy \
        cmake \
        g++ \
        make \
        nodejs \
        npm \
    && npm install --global --no-audit --no-fund \
        markdownlint-cli2@0.17.2 \
        prettier \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

ENTRYPOINT ["/workspace/scripts/container-quality.sh"]
