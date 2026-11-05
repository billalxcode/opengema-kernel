FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
  binutils-aarch64-linux-gnu \
  clang-19 clang-tidy-19 lld-19 llvm-19 llvm-19-tools \
  libclang-rt-19-dev \
  cmake ninja-build \
  dosfstools gdisk mtools parted \
  python3 ca-certificates curl \
  && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-19 100 \
  && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-19 100 \
  && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-19 100 \
  && update-alternatives --install /usr/bin/lld-link lld-link /usr/bin/lld-link-19 100 \
  && update-alternatives --install /usr/bin/llvm-objcopy llvm-objcopy /usr/bin/llvm-objcopy-19 100

WORKDIR /work

CMD ["/bin/bash"]
