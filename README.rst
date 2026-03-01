aria2 — The ultra fast download utility
=======================================

.. image:: https://github.com/Kenshin9977/aria2/actions/workflows/build.yml/badge.svg
   :target: https://github.com/Kenshin9977/aria2/actions/workflows/build.yml
   :alt: CI

.. image:: https://img.shields.io/github/v/release/Kenshin9977/aria2
   :target: https://github.com/Kenshin9977/aria2/releases
   :alt: Release

.. image:: https://img.shields.io/badge/license-GPLv2-blue.svg
   :target: https://github.com/Kenshin9977/aria2/blob/master/COPYING
   :alt: License

Introduction
------------

aria2 is a lightweight, multi-protocol, multi-source, cross-platform
download utility.  It supports HTTP(S), FTP, SFTP, BitTorrent, and
Metalink.  aria2 can download a file from multiple sources/protocols
simultaneously and tries to utilise your maximum download bandwidth.

This is a **modernised fork** of `aria2/aria2
<https://github.com/aria2/aria2>`_ (upstream v1.37.0) with 160+ commits
of improvements.

What's New in This Fork
-----------------------

**New Features**

* HTTP/2 support via nghttp2
* HTTP Digest authentication (RFC 7616)
* Brotli and Zstd content-encoding decompression
* HTTP/3 build system support (ngtcp2 + nghttp3)
* ``io_uring`` event poll backend for Linux
* TTL-based DNS cache expiration
* Cookie ``SameSite`` attribute support
* WebSocket outbound queue limits
* Socket pool size limit with lazy eviction

**Performance & I/O**

* ``pwrite``/``pread`` replacing seek+write/read for I/O operations
* Transfer-Encoding parsed as comma-separated token list

**Security Hardening**

* BitTorrent handshake bounds checking
* HTTP header injection sanitisation
* Error handling for ``fcntl`` and MSE DH computation

**Code Modernisation (C++23)**

* ``std::ranges``, ``operator<=>``, structured bindings, ``enum class``
* ``std::make_unique``, ``[[nodiscard]]``, ``[[maybe_unused]]``
* ``starts_with``/``ends_with``, ``constexpr``, ``= default`` destructors
* ``std::unordered_map`` where ordering not needed
* Lambdas replacing ``std::bind``

**Testing**

* 310+ end-to-end tests across 79 test scripts
* Extended unit test coverage (1300+ tests)
* Integration tests for HTTP, FTP, BT, DHT command pipelines
* ``ISocketCore`` interface with ``MockSocketCore`` for testability

Supported Protocols
-------------------

======================== ===========
Protocol                 Status
======================== ===========
HTTP / HTTPS             Supported
HTTP/2                   Supported
FTP / FTPS               Supported
SFTP                     Supported
BitTorrent               Supported
Metalink (v3 + v4)       Supported
JSON-RPC / XML-RPC       Supported
WebSocket                Supported
SOCKS5 proxy             Supported
HTTP/3 (QUIC)            Build only
======================== ===========

Features
--------

* Command-line interface
* Download files through HTTP(S)/FTP/SFTP/BitTorrent
* Segmented downloading
* Metalink version 4 (RFC 5854) support (HTTP/FTP/SFTP/BitTorrent)
* Metalink version 3.0 support (HTTP/FTP/SFTP/BitTorrent)
* Metalink/HTTP (RFC 6249) support
* HTTP/1.1 and HTTP/2 implementation
* HTTP Proxy support
* HTTP BASIC and Digest authentication support
* HTTP Proxy authentication support
* Well-known environment variables for proxy: ``http_proxy``,
  ``https_proxy``, ``ftp_proxy``, ``all_proxy`` and ``no_proxy``
* HTTP gzip, deflate, Brotli, and Zstd content encoding support
* Verify peer using given trusted CA certificate in HTTPS
* Client certificate authentication in HTTPS
* Chunked transfer encoding support
* Cookie support (Firefox3/Chromium/Netscape format, SameSite attribute)
* Custom HTTP Header support
* Persistent Connections support
* FTP/SFTP through HTTP Proxy
* Download/Upload speed throttling
* BitTorrent extensions: Fast extension, DHT, PEX, MSE/PSE,
  Multi-Tracker, UDP tracker
* BitTorrent `WEB-Seeding <http://getright.com/seedtorrent.html>`_
* BitTorrent Local Peer Discovery
* Rename/change the directory structure of BitTorrent downloads
* JSON-RPC (over HTTP and WebSocket)/XML-RPC interface
* Run as a daemon process
* Selective download in multi-file torrent/Metalink
* Chunk checksum validation in Metalink
* Netrc support
* Configuration file support
* Parameterized URI support
* IPv6 support with Happy Eyeballs
* Disk cache to reduce disk activity

Installation
------------

Pre-built Binaries
~~~~~~~~~~~~~~~~~~

Download the latest release from the `Releases page
<https://github.com/Kenshin9977/aria2/releases>`_.

Building from Source
~~~~~~~~~~~~~~~~~~~~

aria2 is written in C++23.  You need a C++23-capable compiler (GCC 13+,
Clang 17+, MSVC 2022+).

**Dependencies**

======================== ========================================
Feature                  Dependency
======================== ========================================
HTTPS                    OpenSSL, GnuTLS, AppleTLS, or WinTLS
SFTP                     libssh2
BitTorrent               None (optional: libnettle/libgcrypt/OpenSSL)
Metalink                 libxml2 or Expat
Checksum                 None (optional: libnettle/libgcrypt/OpenSSL)
gzip, deflate            zlib
Brotli                   libbrotlidec
Zstd                     libzstd
HTTP/2                   libnghttp2
Async DNS                c-ares
Cookies (SQLite)         libsqlite3
JSON-RPC over WebSocket  libnettle, libgcrypt, or OpenSSL
======================== ========================================

**Quick build**::

    $ autoreconf -i
    $ ./configure
    $ make -j$(nproc)
    $ make check          # run unit tests (requires cppunit)
    $ sudo make install

**macOS (Homebrew)**::

    $ brew install autoconf automake libtool pkgconf openssl@3 \
        c-ares libssh2 sqlite libxml2 cppunit brotli zstd
    $ autoreconf -i
    $ ./configure \
        --with-openssl --without-appletls --without-gnutls \
        --with-libcares --with-sqlite3 --with-libssh2 \
        PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix sqlite)/lib/pkgconfig:$(brew --prefix c-ares)/lib/pkgconfig:$(brew --prefix libssh2)/lib/pkgconfig:$(brew --prefix libxml2)/lib/pkgconfig:$(brew --prefix cppunit)/lib/pkgconfig"
    $ make -j$(sysctl -n hw.ncpu)

Quick Start
-----------

::

    # Download a file
    $ aria2c https://example.com/file.iso

    # Download with 16 connections
    $ aria2c -x16 https://example.com/file.iso

    # Download from multiple mirrors
    $ aria2c https://mirror1.com/file https://mirror2.com/file

    # Download a torrent
    $ aria2c file.torrent

    # Start as JSON-RPC daemon
    $ aria2c --enable-rpc --rpc-listen-all

BitTorrent
----------

About file names
~~~~~~~~~~~~~~~~
The file name of the downloaded file is determined as follows:

single-file mode
    If "name" key is present in .torrent file, the file name is the value
    of "name" key. Otherwise, the file name is the base name of .torrent
    file appended by ".file".  The directory to store the downloaded file
    can be specified by ``-d`` option.

multi-file mode
    The complete directory/file structure mentioned in .torrent file
    is created.  The directory to store the top directory of downloaded
    files can be specified by ``-d`` option.

DHT
~~~

aria2 supports mainline compatible DHT. By default, the routing table
for IPv4 DHT is saved to ``$XDG_CACHE_HOME/aria2/dht.dat`` and the
routing table for IPv6 DHT is saved to
``$XDG_CACHE_HOME/aria2/dht6.dat``.

UDP tracker
~~~~~~~~~~~

UDP tracker support is enabled when IPv4 DHT is enabled.  The port
number of the UDP tracker is shared with DHT. Use ``--dht-listen-port``
option to change the port number.

Metalink
--------

The current implementation supports HTTP(S)/FTP/SFTP/BitTorrent.  Both
Metalink4 (RFC 5854) and Metalink version 3.0 documents are supported.

For checksum verification, md5, sha-1, sha-224, sha-256, sha-384, and
sha-512 are supported.

WebSocket
---------

The WebSocket server embedded in aria2 implements the specification
defined in RFC 6455.  The supported protocol version is 13.

libaria2
--------

The libaria2 is a C++ library that offers aria2 functionality to the
client code.  To enable libaria2, use ``--enable-libaria2`` configure
option.

Disclaimer
----------

This program comes with no warranty.
You must use this program at your own risk.

License
-------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

References
----------

* `aria2 Online Manual <https://aria2.github.io/manual/en/html/>`_
* `RFC 959 FILE TRANSFER PROTOCOL (FTP) <http://tools.ietf.org/html/rfc959>`_
* `RFC 2616 Hypertext Transfer Protocol -- HTTP/1.1 <http://tools.ietf.org/html/rfc2616>`_
* `RFC 5854 The Metalink Download Description Format <http://tools.ietf.org/html/rfc5854>`_
* `RFC 6249 Metalink/HTTP: Mirrors and Hashes <http://tools.ietf.org/html/rfc6249>`_
* `RFC 6455 The WebSocket Protocol <http://tools.ietf.org/html/rfc6455>`_
* `RFC 6555 Happy Eyeballs <http://tools.ietf.org/html/rfc6555>`_
* `RFC 7540 HTTP/2 <http://tools.ietf.org/html/rfc7540>`_
* `RFC 7616 HTTP Digest Access Authentication <http://tools.ietf.org/html/rfc7616>`_
* `The BitTorrent Protocol Specification <http://www.bittorrent.org/beps/bep_0003.html>`_
* `BitTorrent: DHT Protocol <http://www.bittorrent.org/beps/bep_0005.html>`_
* `BitTorrent: Extension Protocol <http://www.bittorrent.org/beps/bep_0010.html>`_
* `Kademlia: A Peer-to-peer Information System Based on the XOR Metric <https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf>`_
