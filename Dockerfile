FROM --platform=linux/amd64 ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential gcc-multilib nasm binutils make python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project
CMD ["make", "image"]
