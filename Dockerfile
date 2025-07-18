FROM alpine:3.22 AS builder

RUN apk add --no-cache  \
    clang               \
    cmake               \
    git                 \
    linux-headers       \
    make                \
    python3-dev         \
                        \
    bzip2-dev           \
    capnproto-dev       \
    expat-dev           \
    libosmium-dev       \
    openssl-dev         \
    protozero-dev       \
    zlib-dev

WORKDIR /usr/src/osmexpress
COPY . /usr/src/osmexpress

RUN cmake -DCMAKE_BUILD_TYPE=Release .
RUN make -j16 && ./osmxTest && make install


FROM alpine:3.22

# libosmium and protozero are header-only libraries, no runtime dependencies.
RUN apk add --no-cache  \
    libbz2              \
    libcrypto3          \
    capnproto           \
    libexpat            \
    libssl3             \
    zlib

COPY --from=builder /usr/local/bin/osmx /usr/local/bin/osmx
ENTRYPOINT [ "/usr/local/bin/osmx" ]
