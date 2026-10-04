FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends g++ && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY hello.cpp .
RUN g++ hello.cpp -o hello

ARG DEFAULT_GREETING=Hello
ENV GREETING=${DEFAULT_GREETING}

ENTRYPOINT ["/workspace/hello"]
CMD ["Docker"]