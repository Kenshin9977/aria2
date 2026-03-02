# Multi-stage build: compile static aria2 binary, then copy to minimal runtime.
#
#   docker build -t aria2 .
#   docker run --rm aria2 --help

# --- Builder ---
FROM alpine:3.21 AS builder

RUN apk add --no-cache \
      build-base autoconf automake libtool pkgconf gettext-dev \
      openssl-dev openssl-libs-static c-ares-dev c-ares-static \
      zlib-dev zlib-static sqlite-dev sqlite-static \
      libssh2-dev libssh2-static libxml2-dev libxml2-static \
      brotli-dev brotli-static zstd-dev zstd-static

COPY . /src
WORKDIR /src

RUN autoreconf -i && \
    ./configure \
      --without-gnutls --with-openssl \
      --with-libcares --with-sqlite3 --with-libssh2 \
      --with-libxml2 --disable-nls \
      ARIA2_STATIC=yes && \
    make -j$(nproc) && \
    strip src/aria2c

# --- Runtime ---
FROM alpine:3.21

RUN apk add --no-cache ca-certificates tini

COPY --from=builder /src/src/aria2c /usr/local/bin/aria2c

ENTRYPOINT ["tini", "--", "aria2c"]
CMD ["--help"]
