cmake_minimum_required(VERSION 3.14)
project(aria2 VERSION 1.37.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckTypeSize)
include(CheckCXXSourceCompiles)
include(CheckCSourceCompiles)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(TestBigEndian)
include(FindPkgConfig)

# ── Host / platform detection ──────────────────────────────────────
if(WIN32 AND MINGW)
  set(MINGW_BUILD ON)
elseif(APPLE)
  set(HAVE_OSX ON)
endif()

# ── Feature options ────────────────────────────────────────────────
option(ENABLE_SSL         "Enable SSL/TLS support"      ON)
option(ENABLE_BITTORRENT  "Enable BitTorrent support"   ON)
option(ENABLE_METALINK    "Enable Metalink support"     ON)
option(ENABLE_WEBSOCKET   "Enable WebSocket support"    ON)
option(ENABLE_EPOLL       "Enable epoll support"        ON)
option(ENABLE_LIBARIA2    "Build libaria2 shared lib"   OFF)
option(ENABLE_WERROR      "Treat warnings as errors"    OFF)
option(ARIA2_STATIC       "Build statically linked"     OFF)

# Library options (default = auto-detect)
option(WITH_APPLETLS   "Use Apple TLS (macOS)"       ON)
option(WITH_WINTLS     "Use Windows TLS"             ON)
option(WITH_GNUTLS     "Use GnuTLS"                  ON)
option(WITH_OPENSSL    "Use OpenSSL"                  ON)
option(WITH_LIBNETTLE  "Use libnettle"                ON)
option(WITH_LIBGMP     "Use libgmp"                   ON)
option(WITH_LIBGCRYPT  "Use libgcrypt"                ON)
option(WITH_LIBXML2    "Use libxml2 for XML parsing"  ON)
option(WITH_LIBEXPAT   "Use expat for XML parsing"    ON)
option(WITH_SQLITE3    "Use SQLite3 for cookies"      ON)
option(WITH_LIBCARES   "Use c-ares for async DNS"     ON)
option(WITH_LIBZ       "Use zlib for compression"     ON)
option(WITH_LIBSSH2    "Use libssh2 for SFTP"         ON)
option(WITH_LIBNGHTTP2 "Use libnghttp2 for HTTP/2"    ON)
option(WITH_LIBUV      "Use libuv event loop"         OFF)
option(WITH_TCMALLOC   "Use tcmalloc"                 OFF)
option(WITH_JEMALLOC   "Use jemalloc"                 OFF)

# ── Build type defaults ────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ── Generate config.h ─────────────────────────────────────────────
set(CONFIG_H_IN "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake")
set(CONFIG_H    "${CMAKE_CURRENT_BINARY_DIR}/config.h")

# ── Platform-specific flags ────────────────────────────────────────
if(MINGW_BUILD)
  add_compile_definitions(_POSIX_C_SOURCE=1)
  if(ARIA2_STATIC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(_USE_32BIT_TIME_T)
  endif()
  link_libraries(ws2_32 wsock32 gdi32 winmm iphlpapi psapi)
  add_link_options(-Wl,--dynamicbase -Wl,--nxcompat)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
   CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-pipe)
endif()

# ── Host info defines ─────────────────────────────────────────────
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}"
       _host_triplet)
set(HAVE_BUILD "${_host_triplet}")
set(HAVE_HOST  "${_host_triplet}")
set(HAVE_TARGET "${_host_triplet}")

# ── pkg-config helper ─────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
if(ARIA2_STATIC)
  set(PKG_CONFIG_ARGN --static)
endif()

# ── Header checks ─────────────────────────────────────────────────
foreach(_hdr
    argz.h arpa/inet.h fcntl.h float.h inttypes.h langinfo.h
    libintl.h limits.h locale.h malloc.h netdb.h netinet/in.h
    netinet/tcp.h poll.h port.h signal.h stddef.h stdint.h
    stdio_ext.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h
    sys/resource.h sys/signal.h sys/socket.h sys/time.h sys/types.h
    sys/uio.h sys/utsname.h termios.h unistd.h utime.h wchar.h
    ifaddrs.h pwd.h)
  string(MAKE_C_IDENTIFIER "HAVE_${_hdr}" _var)
  string(TOUPPER "${_var}" _var)
  check_include_file("${_hdr}" ${_var})
endforeach()

# ── Function checks ───────────────────────────────────────────────
foreach(_fn
    atexit ftruncate getcwd getentropy gethostbyaddr gethostbyname
    getifaddrs getpagesize memchr memmove mempcpy memset mkdir
    mmap munmap nl_langinfo posix_fadvise posix_memalign pow putenv
    rmdir select setlocale sigaction sleep socket stpcpy strcasecmp
    strchr strcspn strdup strerror strncasecmp strstr strtol strtoul
    strtoull tzset unsetenv usleep utime utimes
    asctime_r localtime_r basename gai_strerror getaddrinfo
    gettimeofday strptime daemon poll timegm
    posix_fallocate epoll_create port_associate kqueue)
  string(MAKE_C_IDENTIFIER "HAVE_${_fn}" _var)
  string(TOUPPER "${_var}" _var)
  check_function_exists("${_fn}" ${_var})
endforeach()

# ── Type / struct checks ──────────────────────────────────────────
check_type_size("ptrdiff_t" PTRDIFF_T)
test_big_endian(WORDS_BIGENDIAN)

check_cxx_source_compiles("
  #include <time.h>
  int main() { struct timespec ts; return 0; }
" HAVE_A2_STRUCT_TIMESPEC)

if(NOT MINGW_BUILD)
  check_struct_has_member("struct sockaddr_in" sin_len
    "sys/types.h;sys/socket.h;netinet/in.h"
    HAVE_SOCKADDR_IN_SIN_LEN)
  check_struct_has_member("struct sockaddr_in6" sin6_len
    "sys/types.h;sys/socket.h;netinet/in.h"
    HAVE_SOCKADDR_IN6_SIN6_LEN)
endif()

# kqueue udata type
if(HAVE_KQUEUE)
  check_cxx_source_compiles("
    #include <sys/types.h>
    #include <sys/event.h>
    #include <sys/time.h>
    int main() {
      struct kevent ev;
      ev.udata = reinterpret_cast<intptr_t>(&ev);
      return 0;
    }
  " KEVENT_UDATA_INTPTR_T)
endif()

# struct option.name const-ness
check_cxx_source_compiles("
  #include <unistd.h>
  #include <getopt.h>
  int main() {
    const char* s = \"test\";
    option op;
    op.name = s;
    return 0;
  }
" HAVE_OPTION_CONST_NAME)

# ── fallocate detection ────────────────────────────────────────────
set(HAVE_SOME_FALLOCATE OFF)
if(HAVE_POSIX_FALLOCATE OR HAVE_OSX OR MINGW_BUILD)
  set(HAVE_SOME_FALLOCATE ON)
endif()
if(NOT HAVE_SOME_FALLOCATE)
  check_function_exists(fallocate HAVE_FALLOCATE)
  if(HAVE_FALLOCATE)
    set(HAVE_SOME_FALLOCATE ON)
  endif()
endif()

# ── epoll detection ────────────────────────────────────────────────
if(NOT ENABLE_EPOLL)
  set(HAVE_EPOLL_CREATE OFF)
endif()
if(HAVE_EPOLL_CREATE)
  set(HAVE_EPOLL ON)
endif()

# ── io_uring detection ────────────────────────────────────────────
set(HAVE_IOURING OFF)
pkg_check_modules(LIBURING liburing>=2.0)
if(LIBURING_FOUND)
  set(HAVE_IOURING ON)
endif()

# ── Library detection ──────────────────────────────────────────────

# CppUnit (for tests)
pkg_check_modules(CPPUNIT cppunit>=1.10.2)

# zlib
set(HAVE_ZLIB OFF)
if(WITH_LIBZ)
  pkg_check_modules(ZLIB zlib>=1.2.3)
  if(ZLIB_FOUND)
    set(HAVE_ZLIB ON)
  else()
    find_library(ZLIB_LIBRARY z)
    if(ZLIB_LIBRARY)
      set(HAVE_ZLIB ON)
      set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
      set(ZLIB_INCLUDE_DIRS "")
    endif()
  endif()
endif()

# libuv
set(HAVE_LIBUV OFF)
if(WITH_LIBUV)
  pkg_check_modules(LIBUV libuv>=1.13)
  if(LIBUV_FOUND)
    set(HAVE_LIBUV ON)
  endif()
endif()

# libxml2
set(HAVE_LIBXML2 OFF)
if(WITH_LIBXML2)
  pkg_check_modules(LIBXML2 libxml-2.0>=2.6.24)
  if(LIBXML2_FOUND)
    set(HAVE_LIBXML2 ON)
  endif()
endif()

# expat (only if libxml2 not found)
set(HAVE_LIBEXPAT OFF)
if(WITH_LIBEXPAT AND NOT HAVE_LIBXML2)
  pkg_check_modules(EXPAT expat)
  if(EXPAT_FOUND)
    set(HAVE_LIBEXPAT ON)
  endif()
endif()

# SQLite3
set(HAVE_SQLITE3 OFF)
if(WITH_SQLITE3)
  pkg_check_modules(SQLITE3 sqlite3)
  if(SQLITE3_FOUND)
    set(HAVE_SQLITE3 ON)
  endif()
endif()

# ── SSL/TLS detection (priority: Apple > Win > GnuTLS > OpenSSL) ──
set(HAVE_SSL OFF)
set(HAVE_APPLETLS OFF)
set(HAVE_WINTLS OFF)
set(HAVE_LIBGNUTLS OFF)
set(HAVE_OPENSSL OFF)

if(NOT ENABLE_SSL)
  set(WITH_APPLETLS OFF)
  set(WITH_WINTLS OFF)
  set(WITH_GNUTLS OFF)
  set(WITH_OPENSSL OFF)
  set(WITH_LIBNETTLE OFF)
  set(WITH_LIBGCRYPT OFF)
endif()

# Apple TLS (macOS native)
if(WITH_APPLETLS AND HAVE_OSX)
  set(HAVE_APPLETLS ON)
  set(HAVE_SSL ON)
  set(APPLETLS_LDFLAGS "-framework CoreFoundation -framework Security")
endif()

# Windows TLS
if(WITH_WINTLS AND MINGW_BUILD AND NOT HAVE_SSL)
  find_library(_crypt32 crypt32)
  find_library(_secur32 secur32)
  find_library(_advapi32 advapi32)
  if(_crypt32 AND _secur32 AND _advapi32)
    set(HAVE_WINTLS ON)
    set(HAVE_SSL ON)
    set(WINTLS_LIBRARIES crypt32 secur32 advapi32)
  endif()
endif()

# GnuTLS
if(WITH_GNUTLS AND NOT HAVE_SSL)
  pkg_check_modules(LIBGNUTLS gnutls>=2.2.0)
  if(LIBGNUTLS_FOUND)
    set(HAVE_LIBGNUTLS ON)
    set(HAVE_SSL ON)
  endif()
endif()

# OpenSSL
if(WITH_OPENSSL AND NOT HAVE_SSL)
  pkg_check_modules(OPENSSL openssl>=0.9.8)
  if(OPENSSL_FOUND)
    set(HAVE_OPENSSL ON)
    set(HAVE_SSL ON)
  endif()
endif()

# Nettle, GMP, Gcrypt (for non-OpenSSL crypto)
set(HAVE_LIBNETTLE OFF)
set(HAVE_LIBGMP OFF)
set(HAVE_LIBGCRYPT OFF)

if(NOT HAVE_OPENSSL)
  if(WITH_LIBNETTLE AND NOT HAVE_APPLETLS AND NOT HAVE_WINTLS)
    pkg_check_modules(LIBNETTLE nettle)
    if(LIBNETTLE_FOUND)
      set(HAVE_LIBNETTLE ON)
    endif()
  endif()
  if(WITH_LIBGMP AND (HAVE_LIBNETTLE OR HAVE_APPLETLS OR HAVE_WINTLS)
     AND ENABLE_BITTORRENT)
    find_library(LIBGMP_LIBRARY gmp)
    if(LIBGMP_LIBRARY)
      set(HAVE_LIBGMP ON)
      set(LIBGMP_LIBRARIES ${LIBGMP_LIBRARY})
    endif()
  endif()
  if(WITH_LIBGCRYPT AND NOT HAVE_APPLETLS AND NOT HAVE_WINTLS
     AND NOT HAVE_LIBNETTLE)
    pkg_check_modules(LIBGCRYPT libgcrypt>=1.2.4)
    if(LIBGCRYPT_FOUND)
      set(HAVE_LIBGCRYPT ON)
    endif()
  endif()
endif()

# libssh2
set(HAVE_LIBSSH2 OFF)
if(WITH_LIBSSH2)
  pkg_check_modules(LIBSSH2 libssh2)
  if(LIBSSH2_FOUND)
    set(HAVE_LIBSSH2 ON)
  endif()
endif()

# libnghttp2
set(HAVE_LIBNGHTTP2 OFF)
if(WITH_LIBNGHTTP2)
  pkg_check_modules(LIBNGHTTP2 libnghttp2>=1.0.0)
  if(LIBNGHTTP2_FOUND)
    set(HAVE_LIBNGHTTP2 ON)
  endif()
endif()

# c-ares
set(HAVE_LIBCARES OFF)
if(WITH_LIBCARES)
  pkg_check_modules(LIBCARES libcares>=1.16.0)
  if(LIBCARES_FOUND)
    set(HAVE_LIBCARES ON)
  endif()
endif()

# tcmalloc
set(HAVE_TCMALLOC OFF)
if(WITH_TCMALLOC)
  pkg_check_modules(TCMALLOC libtcmalloc_minimal)
  if(NOT TCMALLOC_FOUND)
    find_library(TCMALLOC_LIBRARY tcmalloc_minimal)
    if(TCMALLOC_LIBRARY)
      set(HAVE_TCMALLOC ON)
      set(TCMALLOC_LIBRARIES ${TCMALLOC_LIBRARY})
    endif()
  else()
    set(HAVE_TCMALLOC ON)
  endif()
endif()

# jemalloc
set(HAVE_JEMALLOC OFF)
if(WITH_JEMALLOC)
  pkg_check_modules(JEMALLOC jemalloc)
  if(NOT JEMALLOC_FOUND)
    find_library(JEMALLOC_LIBRARY jemalloc)
    if(JEMALLOC_LIBRARY)
      set(HAVE_JEMALLOC ON)
      set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
    endif()
  else()
    set(HAVE_JEMALLOC ON)
  endif()
endif()

# ── Derived feature flags ─────────────────────────────────────────
set(ENABLE_ASYNC_DNS ${HAVE_LIBCARES})

# Metalink requires an XML library
if(ENABLE_METALINK AND NOT HAVE_LIBXML2 AND NOT HAVE_LIBEXPAT)
  set(ENABLE_METALINK OFF)
endif()

# XML-RPC requires an XML library
set(ENABLE_XML_RPC OFF)
if(HAVE_LIBXML2 OR HAVE_LIBEXPAT)
  set(ENABLE_XML_RPC ON)
endif()
set(HAVE_SOME_XMLLIB ${ENABLE_XML_RPC})

# Message digest implementation selection
if(HAVE_APPLETLS)
  set(USE_APPLE_MD ON)
elseif(HAVE_LIBNETTLE)
  set(USE_LIBNETTLE_MD ON)
elseif(HAVE_LIBGCRYPT)
  set(USE_LIBGCRYPT_MD ON)
elseif(HAVE_OPENSSL)
  set(USE_OPENSSL_MD ON)
else()
  set(USE_INTERNAL_MD ON)
endif()

# Bignum implementation
if(HAVE_LIBGMP OR HAVE_LIBGCRYPT OR HAVE_OPENSSL)
  set(USE_INTERNAL_BIGNUM OFF)
else()
  set(USE_INTERNAL_BIGNUM ON)
endif()

# ARC4 implementation
if(HAVE_LIBNETTLE OR HAVE_LIBGCRYPT OR HAVE_OPENSSL)
  set(USE_INTERNAL_ARC4 OFF)
else()
  set(USE_INTERNAL_ARC4 ON)
endif()

# ── Generate config.h ─────────────────────────────────────────────
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

# ── Core source files ─────────────────────────────────────────────
set(ARIA2_SOURCES
  src/A2STR.cc
  src/AbstractAuthResolver.cc
  src/AbstractCommand.cc
  src/AbstractDiskWriter.cc
  src/AbstractHttpServerResponseCommand.cc
  src/AbstractOptionHandler.cc
  src/AbstractProxyRequestCommand.cc
  src/AbstractProxyResponseCommand.cc
  src/AbstractSingleDiskAdaptor.cc
  src/AdaptiveFileAllocationIterator.cc
  src/AdaptiveURISelector.cc
  src/AuthConfig.cc
  src/AuthConfigFactory.cc
  src/AutoSaveCommand.cc
  src/BackupIPv4ConnectCommand.cc
  src/base32.cc
  src/bitfield.cc
  src/BitfieldMan.cc
  src/BufferedFile.cc
  src/ByteArrayDiskWriter.cc
  src/CheckIntegrityCommand.cc
  src/CheckIntegrityDispatcherCommand.cc
  src/CheckIntegrityEntry.cc
  src/Checksum.cc
  src/ChecksumCheckIntegrityEntry.cc
  src/ChunkChecksum.cc
  src/ChunkedDecodingStreamFilter.cc
  src/ColorizedStream.cc
  src/Command.cc
  src/ConnectCommand.cc
  src/console.cc
  src/ConsoleStatCalc.cc
  src/ContentTypeRequestGroupCriteria.cc
  src/Context.cc
  src/ContextAttribute.cc
  src/Cookie.cc
  src/CookieStorage.cc
  src/cookie_helper.cc
  src/CreateRequestCommand.cc
  src/CUIDCounter.cc
  src/DefaultAuthResolver.cc
  src/DefaultBtProgressInfoFile.cc
  src/DefaultDiskWriter.cc
  src/DefaultDiskWriterFactory.cc
  src/DefaultPieceStorage.cc
  src/DefaultStreamPieceSelector.cc
  src/DirectDiskAdaptor.cc
  src/DiskAdaptor.cc
  src/DlAbortEx.cc
  src/DlRetryEx.cc
  src/DNSCache.cc
  src/DownloadCommand.cc
  src/DownloadContext.cc
  src/DownloadEngine.cc
  src/DownloadEngineFactory.cc
  src/DownloadFailureException.cc
  src/DownloadHandler.cc
  src/DownloadHandlerConstants.cc
  src/DownloadResult.cc
  src/download_handlers.cc
  src/download_helper.cc
  src/Exception.cc
  src/FatalException.cc
  src/FeatureConfig.cc
  src/FeedbackURISelector.cc
  src/File.cc
  src/FileAllocationCommand.cc
  src/FileAllocationDispatcherCommand.cc
  src/FileAllocationEntry.cc
  src/FileEntry.cc
  src/FillRequestGroupCommand.cc
  src/fmt.cc
  src/FtpConnection.cc
  src/FtpDownloadCommand.cc
  src/FtpFinishDownloadCommand.cc
  src/FtpInitiateConnectionCommand.cc
  src/FtpNegotiationCommand.cc
  src/FtpTunnelRequestCommand.cc
  src/FtpTunnelResponseCommand.cc
  src/GeomStreamPieceSelector.cc
  src/GroupId.cc
  src/GrowSegment.cc
  src/HaveEraseCommand.cc
  src/help_tags.cc
  src/HttpConnection.cc
  src/HttpDownloadCommand.cc
  src/HttpHeader.cc
  src/HttpHeaderProcessor.cc
  src/HttpInitiateConnectionCommand.cc
  src/HttpListenCommand.cc
  src/HttpProxyRequestCommand.cc
  src/HttpProxyResponseCommand.cc
  src/HttpRequest.cc
  src/HttpRequestCommand.cc
  src/HttpResponse.cc
  src/HttpResponseCommand.cc
  src/HttpServer.cc
  src/HttpServerBodyCommand.cc
  src/HttpServerCommand.cc
  src/HttpServerResponseCommand.cc
  src/HttpSkipResponseCommand.cc
  src/HstsStore.cc
  src/InitiateConnectionCommand.cc
  src/InitiateConnectionCommandFactory.cc
  src/InorderStreamPieceSelector.cc
  src/RandomStreamPieceSelector.cc
  src/InorderURISelector.cc
  src/IOFile.cc
  src/IteratableChecksumValidator.cc
  src/IteratableChunkChecksumValidator.cc
  src/json.cc
  src/JsonParser.cc
  src/LogFactory.cc
  src/Logger.cc
  src/LongestSequencePieceSelector.cc
  src/MessageDigest.cc
  src/message_digest_helper.cc
  src/MetadataInfo.cc
  src/MetalinkHttpEntry.cc
  src/MultiDiskAdaptor.cc
  src/MultiFileAllocationIterator.cc
  src/MultiUrlRequestInfo.cc
  src/NameResolver.cc
  src/Netrc.cc
  src/NetrcAuthResolver.cc
  src/NetStat.cc
  src/Notifier.cc
  src/NsCookieParser.cc
  src/NullSinkStreamFilter.cc
  src/Option.cc
  src/OptionHandler.cc
  src/OptionHandlerException.cc
  src/OptionHandlerFactory.cc
  src/OptionHandlerImpl.cc
  src/OptionParser.cc
  src/option_processing.cc
  src/paramed_string.cc
  src/PeerStat.cc
  src/Piece.cc
  src/PiecedSegment.cc
  src/PieceHashCheckIntegrityEntry.cc
  src/PieceStatMan.cc
  src/Platform.cc
  src/prefs.cc
  src/ProtocolDetector.cc
  src/Range.cc
  src/RarestPieceSelector.cc
  src/RealtimeCommand.cc
  src/RecoverableException.cc
  src/Request.cc
  src/RequestGroup.cc
  src/RequestGroupEntry.cc
  src/RequestGroupMan.cc
  src/RpcMethod.cc
  src/RpcMethodFactory.cc
  src/RpcMethodImpl.cc
  src/RpcRequest.cc
  src/RpcResponse.cc
  src/rpc_helper.cc
  src/SaveSessionCommand.cc
  src/SegmentMan.cc
  src/SelectEventPoll.cc
  src/ServerStat.cc
  src/ServerStatMan.cc
  src/SessionSerializer.cc
  src/Signature.cc
  src/SimpleRandomizer.cc
  src/SingleFileAllocationIterator.cc
  src/SinkStreamFilter.cc
  src/Socks5ConnectCommand.cc
  src/Socks5HandshakeCommand.cc
  src/SocketBuffer.cc
  src/SocketCore.cc
  src/SocketRecvBuffer.cc
  src/SpeedCalc.cc
  src/StreamCheckIntegrityEntry.cc
  src/StreamFileAllocationEntry.cc
  src/StreamFilter.cc
  src/TimeA2.cc
  src/TimeBasedCommand.cc
  src/TimedHaltCommand.cc
  src/TimerA2.cc
  src/TorrentAttribute.cc
  src/TransferStat.cc
  src/TruncFileAllocationIterator.cc
  src/UnknownLengthPieceStorage.cc
  src/UnknownOptionException.cc
  src/uri.cc
  src/UriListParser.cc
  src/URIResult.cc
  src/uri_split.c
  src/util.cc
  src/util_security.cc
  src/ValueBase.cc
  src/ValueBaseStructParserStateImpl.cc
  src/ValueBaseStructParserStateMachine.cc
  src/version_usage.cc
  src/wallclock.cc
  src/WatchProcessCommand.cc
  src/WrDiskCache.cc
  src/WrDiskCacheEntry.cc
  src/XmlRpcRequestParserController.cc
  src/OpenedFileCounter.cc
  src/SHA1IOFile.cc
  src/EvictSocketPoolCommand.cc
)

# ── Conditional sources ────────────────────────────────────────────

# MinGW
if(MINGW_BUILD)
  list(APPEND ARIA2_SOURCES src/WinConsoleFile.cc)
endif()

# WebSocket (wslay)
if(ENABLE_WEBSOCKET)
  list(APPEND ARIA2_SOURCES
    src/WebSocketInteractionCommand.cc
    src/WebSocketResponseCommand.cc
    src/WebSocketSession.cc
    src/WebSocketSessionMan.cc
  )
  # Build bundled wslay as a static library
  add_library(wslay STATIC
    deps/wslay/lib/wslay_event.c
    deps/wslay/lib/wslay_frame.c
    deps/wslay/lib/wslay_net.c
    deps/wslay/lib/wslay_queue.c
    deps/wslay/lib/wslay_stack.c
  )
  target_include_directories(wslay PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/wslay/lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/wslay/lib"
  )
  set(WSLAY_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/wslay/lib/includes"
  )
endif()

# XML libraries
if(HAVE_SOME_XMLLIB)
  list(APPEND ARIA2_SOURCES
    src/XmlAttr.cc
    src/XmlParser.cc
  )
endif()
if(HAVE_LIBXML2)
  list(APPEND ARIA2_SOURCES src/Xml2XmlParser.cc)
endif()
if(HAVE_LIBEXPAT)
  list(APPEND ARIA2_SOURCES src/ExpatXmlParser.cc)
endif()

# XML-RPC
if(ENABLE_XML_RPC)
  list(APPEND ARIA2_SOURCES
    src/XmlRpcDiskWriter.cc
    src/XmlRpcRequestParserStateImpl.cc
    src/XmlRpcRequestParserStateMachine.cc
  )
endif()

# fallocate
if(HAVE_SOME_FALLOCATE)
  list(APPEND ARIA2_SOURCES src/FallocFileAllocationIterator.cc)
endif()

# epoll
if(HAVE_EPOLL)
  list(APPEND ARIA2_SOURCES src/EpollEventPoll.cc)
endif()

# io_uring
if(HAVE_IOURING)
  list(APPEND ARIA2_SOURCES src/IouringEventPoll.cc)
endif()

# SSL/TLS backends
if(HAVE_APPLETLS)
  list(APPEND ARIA2_SOURCES
    src/AppleTLSContext.cc
    src/AppleTLSSession.cc
  )
endif()
if(HAVE_WINTLS)
  list(APPEND ARIA2_SOURCES
    src/WinTLSContext.cc
    src/WinTLSSession.cc
  )
endif()
if(HAVE_LIBGNUTLS)
  list(APPEND ARIA2_SOURCES
    src/LibgnutlsTLSContext.cc
    src/LibgnutlsTLSSession.cc
  )
endif()
if(HAVE_OPENSSL AND NOT HAVE_APPLETLS)
  list(APPEND ARIA2_SOURCES
    src/LibsslTLSContext.cc
    src/LibsslTLSSession.cc
  )
endif()

# Message digest implementations
if(USE_APPLE_MD)
  list(APPEND ARIA2_SOURCES src/AppleMessageDigestImpl.cc)
endif()
if(USE_INTERNAL_MD)
  list(APPEND ARIA2_SOURCES
    src/InternalMessageDigestImpl.cc
    src/crypto_hash.cc
  )
endif()
if(USE_OPENSSL_MD)
  list(APPEND ARIA2_SOURCES src/LibsslMessageDigestImpl.cc)
endif()
if(USE_LIBNETTLE_MD)
  list(APPEND ARIA2_SOURCES src/LibnettleMessageDigestImpl.cc)
endif()
if(USE_LIBGCRYPT_MD)
  list(APPEND ARIA2_SOURCES src/LibgcryptMessageDigestImpl.cc)
endif()

# Crypto implementations
if(USE_INTERNAL_BIGNUM)
  list(APPEND ARIA2_SOURCES src/InternalDHKeyExchange.cc)
endif()
if(USE_INTERNAL_ARC4)
  list(APPEND ARIA2_SOURCES src/InternalARC4Encryptor.cc)
endif()
if(HAVE_OPENSSL)
  list(APPEND ARIA2_SOURCES
    src/LibsslARC4Encryptor.cc
    src/LibsslDHKeyExchange.cc
  )
endif()
if(HAVE_LIBNETTLE)
  list(APPEND ARIA2_SOURCES src/LibnettleARC4Encryptor.cc)
endif()
if(HAVE_LIBGCRYPT)
  list(APPEND ARIA2_SOURCES
    src/LibgcryptARC4Encryptor.cc
    src/LibgcryptDHKeyExchange.cc
  )
endif()
if(HAVE_LIBGMP)
  list(APPEND ARIA2_SOURCES
    src/a2gmp.cc
    src/LibgmpDHKeyExchange.cc
  )
endif()

# zlib
if(HAVE_ZLIB)
  list(APPEND ARIA2_SOURCES
    src/GZipDecodingStreamFilter.cc
    src/GZipEncoder.cc
    src/GZipFile.cc
    src/Adler32MessageDigestImpl.cc
  )
endif()

# SQLite3
if(HAVE_SQLITE3)
  list(APPEND ARIA2_SOURCES
    src/Sqlite3CookieParser.cc
    src/Sqlite3CookieParserImpl.cc
  )
endif()

# libssh2 (SFTP)
if(HAVE_LIBSSH2)
  list(APPEND ARIA2_SOURCES
    src/SSHSession.cc
    src/SftpNegotiationCommand.cc
    src/SftpDownloadCommand.cc
    src/SftpFinishDownloadCommand.cc
  )
endif()

# libnghttp2 (HTTP/2)
if(HAVE_LIBNGHTTP2)
  list(APPEND ARIA2_SOURCES
    src/Http2Session.cc
    src/Http2RequestCommand.cc
  )
endif()

# c-ares (async DNS)
if(ENABLE_ASYNC_DNS)
  list(APPEND ARIA2_SOURCES
    src/AsyncNameResolver.cc
    src/AsyncNameResolverMan.cc
  )
endif()

# BitTorrent
if(ENABLE_BITTORRENT)
  list(APPEND ARIA2_SOURCES
    src/AbstractBtMessage.cc
    src/ActivePeerConnectionCommand.cc
    src/AnnounceList.cc
    src/AnnounceTier.cc
    src/bencode2.cc
    src/BencodeParser.cc
    src/bittorrent_helper.cc
    src/BtAbortOutstandingRequestEvent.cc
    src/BtAllowedFastMessage.cc
    src/BtAnnounce.cc
    src/BtBitfieldMessage.cc
    src/BtBitfieldMessageValidator.cc
    src/BtCancelMessage.cc
    src/BtCheckIntegrityEntry.cc
    src/BtChokeMessage.cc
    src/BtDependency.cc
    src/BtExtendedMessage.cc
    src/BtFileAllocationEntry.cc
    src/BtHandshakeMessage.cc
    src/BtHandshakeMessageValidator.cc
    src/BtHaveAllMessage.cc
    src/BtHaveMessage.cc
    src/BtHaveNoneMessage.cc
    src/BtInterestedMessage.cc
    src/BtKeepAliveMessage.cc
    src/BtLeecherStateChoke.cc
    src/BtNotInterestedMessage.cc
    src/BtPieceMessage.cc
    src/BtPieceMessageValidator.cc
    src/BtPortMessage.cc
    src/BtPostDownloadHandler.cc
    src/BtRegistry.cc
    src/BtRejectMessage.cc
    src/BtRequestMessage.cc
    src/BtRuntime.cc
    src/BtSeederStateChoke.cc
    src/BtSetup.cc
    src/BtStopDownloadCommand.cc
    src/BtSuggestPieceMessage.cc
    src/BtUnchokeMessage.cc
    src/DefaultBtAnnounce.cc
    src/DefaultBtInteractive.cc
    src/DefaultBtMessageDispatcher.cc
    src/DefaultBtMessageFactory.cc
    src/DefaultBtMessageReceiver.cc
    src/DefaultBtRequestFactory.cc
    src/DefaultExtensionMessageFactory.cc
    src/DefaultPeerStorage.cc
    src/DHTAbstractMessage.cc
    src/DHTAbstractTask.cc
    src/DHTAnnouncePeerMessage.cc
    src/DHTAnnouncePeerReplyMessage.cc
    src/DHTAutoSaveCommand.cc
    src/DHTBucket.cc
    src/DHTBucketRefreshCommand.cc
    src/DHTBucketRefreshTask.cc
    src/DHTBucketTree.cc
    src/DHTConnectionImpl.cc
    src/DHTEntryPointNameResolveCommand.cc
    src/DHTFindNodeMessage.cc
    src/DHTFindNodeReplyMessage.cc
    src/DHTGetPeersCommand.cc
    src/DHTGetPeersMessage.cc
    src/DHTGetPeersReplyMessage.cc
    src/DHTInteractionCommand.cc
    src/DHTMessage.cc
    src/DHTMessageDispatcherImpl.cc
    src/DHTMessageEntry.cc
    src/DHTMessageFactoryImpl.cc
    src/DHTMessageReceiver.cc
    src/DHTMessageTracker.cc
    src/DHTMessageTrackerEntry.cc
    src/DHTNode.cc
    src/DHTNodeLookupEntry.cc
    src/DHTNodeLookupTask.cc
    src/DHTNodeLookupTaskCallback.cc
    src/DHTPeerAnnounceCommand.cc
    src/DHTPeerAnnounceEntry.cc
    src/DHTPeerAnnounceStorage.cc
    src/DHTPeerLookupTask.cc
    src/DHTPeerLookupTaskCallback.cc
    src/DHTPingMessage.cc
    src/DHTPingReplyMessage.cc
    src/DHTPingTask.cc
    src/DHTQueryMessage.cc
    src/DHTRegistry.cc
    src/DHTReplaceNodeTask.cc
    src/DHTResponseMessage.cc
    src/DHTRoutingTable.cc
    src/DHTRoutingTableDeserializer.cc
    src/DHTRoutingTableSerializer.cc
    src/DHTSetup.cc
    src/DHTTaskExecutor.cc
    src/DHTTaskFactoryImpl.cc
    src/DHTTaskQueueImpl.cc
    src/DHTTokenTracker.cc
    src/DHTTokenUpdateCommand.cc
    src/DHTUnknownMessage.cc
    src/ExtensionMessageRegistry.cc
    src/HandshakeExtensionMessage.cc
    src/IndexBtMessage.cc
    src/IndexBtMessageValidator.cc
    src/InitiatorMSEHandshakeCommand.cc
    src/LpdDispatchMessageCommand.cc
    src/LpdMessage.cc
    src/LpdMessageDispatcher.cc
    src/LpdMessageReceiver.cc
    src/LpdReceiveMessageCommand.cc
    src/magnet.cc
    src/MSEHandshake.cc
    src/NameResolveCommand.cc
    src/Peer.cc
    src/PeerAbstractCommand.cc
    src/PeerAddrEntry.cc
    src/PeerChokeCommand.cc
    src/PeerConnection.cc
    src/PeerInitiateConnectionCommand.cc
    src/PeerInteractionCommand.cc
    src/PeerListenCommand.cc
    src/PeerReceiveHandshakeCommand.cc
    src/PeerSessionResource.cc
    src/PriorityPieceSelector.cc
    src/RangeBtMessage.cc
    src/RangeBtMessageValidator.cc
    src/ReceiverMSEHandshakeCommand.cc
    src/RequestSlot.cc
    src/SeedCheckCommand.cc
    src/ShareRatioSeedCriteria.cc
    src/SimpleBtMessage.cc
    src/TimeSeedCriteria.cc
    src/TrackerWatcherCommand.cc
    src/UDPTrackerClient.cc
    src/UDPTrackerRequest.cc
    src/UnionSeedCriteria.cc
    src/UTMetadataDataExtensionMessage.cc
    src/UTMetadataExtensionMessage.cc
    src/UTMetadataPostDownloadHandler.cc
    src/UTMetadataRejectExtensionMessage.cc
    src/UTMetadataRequestExtensionMessage.cc
    src/UTMetadataRequestFactory.cc
    src/UTMetadataRequestTracker.cc
    src/UTPexExtensionMessage.cc
    src/ZeroBtMessage.cc
  )
endif()

# Metalink
if(ENABLE_METALINK)
  list(APPEND ARIA2_SOURCES
    src/Metalink2RequestGroup.cc
    src/MetalinkEntry.cc
    src/Metalinker.cc
    src/MetalinkMetaurl.cc
    src/MetalinkParserController.cc
    src/MetalinkParserState.cc
    src/MetalinkParserStateImpl.cc
    src/MetalinkParserStateMachine.cc
    src/MetalinkParserStateV3Impl.cc
    src/MetalinkParserStateV4Impl.cc
    src/MetalinkPostDownloadHandler.cc
    src/MetalinkResource.cc
    src/metalink_helper.cc
  )
endif()

# Portability shims for missing functions
if(NOT HAVE_ASCTIME_R)
  list(APPEND ARIA2_SOURCES src/asctime_r.c)
endif()
if(NOT HAVE_BASENAME)
  list(APPEND ARIA2_SOURCES src/libgen.c)
endif()
if(NOT HAVE_GETADDRINFO AND NOT MINGW_BUILD)
  list(APPEND ARIA2_SOURCES src/getaddrinfo.c)
endif()
if(NOT HAVE_GAI_STRERROR)
  list(APPEND ARIA2_SOURCES src/gai_strerror.c)
endif()
if(NOT HAVE_GETTIMEOFDAY)
  list(APPEND ARIA2_SOURCES src/gettimeofday.c)
endif()
if(NOT HAVE_LOCALTIME_R)
  list(APPEND ARIA2_SOURCES src/localtime_r.c)
endif()
if(NOT HAVE_STRPTIME)
  list(APPEND ARIA2_SOURCES src/strptime.c)
endif()
if(NOT HAVE_TIMEGM)
  list(APPEND ARIA2_SOURCES src/timegm.c)
endif()
if(NOT HAVE_DAEMON)
  list(APPEND ARIA2_SOURCES src/daemon.cc)
endif()

# Event poll backends
if(HAVE_POLL)
  list(APPEND ARIA2_SOURCES src/PollEventPoll.cc)
endif()
if(HAVE_PORT_ASSOCIATE)
  list(APPEND ARIA2_SOURCES src/PortEventPoll.cc)
endif()
if(HAVE_KQUEUE)
  list(APPEND ARIA2_SOURCES src/KqueueEventPoll.cc)
endif()
if(HAVE_LIBUV)
  list(APPEND ARIA2_SOURCES src/LibuvEventPoll.cc)
endif()

# libaria2 API
if(ENABLE_LIBARIA2)
  list(APPEND ARIA2_SOURCES
    src/ApiCallbackDownloadEventListener.cc
    src/aria2api.cc
    src/KeepRunningCommand.cc
  )
endif()

# ── Build libaria2 library ─────────────────────────────────────────
if(ENABLE_LIBARIA2)
  add_library(aria2_lib SHARED ${ARIA2_SOURCES})
  set_target_properties(aria2_lib PROPERTIES
    OUTPUT_NAME aria2
    VERSION   ${PROJECT_VERSION}
    SOVERSION 0
  )
else()
  add_library(aria2_lib STATIC ${ARIA2_SOURCES})
  set_target_properties(aria2_lib PROPERTIES
    OUTPUT_NAME aria2
  )
endif()

# ── Include directories ───────────────────────────────────────────
target_include_directories(aria2_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/includes>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src/includes>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(aria2_lib PRIVATE
  LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
  HAVE_CONFIG_H
)

# ── Link libraries ────────────────────────────────────────────────
macro(aria2_link_pkg TARGET PKG)
  if(${PKG}_INCLUDE_DIRS)
    target_include_directories(${TARGET} PUBLIC ${${PKG}_INCLUDE_DIRS})
  endif()
  if(${PKG}_LIBRARIES)
    target_link_libraries(${TARGET} PUBLIC ${${PKG}_LIBRARIES})
  endif()
  if(${PKG}_LIBRARY_DIRS)
    target_link_directories(${TARGET} PUBLIC ${${PKG}_LIBRARY_DIRS})
  endif()
endmacro()

if(HAVE_ZLIB)
  aria2_link_pkg(aria2_lib ZLIB)
endif()
if(HAVE_LIBUV)
  aria2_link_pkg(aria2_lib LIBUV)
endif()
if(HAVE_LIBXML2)
  aria2_link_pkg(aria2_lib LIBXML2)
endif()
if(HAVE_LIBEXPAT)
  aria2_link_pkg(aria2_lib EXPAT)
endif()
if(HAVE_SQLITE3)
  aria2_link_pkg(aria2_lib SQLITE3)
endif()
if(HAVE_LIBGNUTLS)
  aria2_link_pkg(aria2_lib LIBGNUTLS)
endif()
if(HAVE_OPENSSL)
  aria2_link_pkg(aria2_lib OPENSSL)
endif()
if(HAVE_LIBNETTLE)
  aria2_link_pkg(aria2_lib LIBNETTLE)
endif()
if(HAVE_LIBGMP)
  target_link_libraries(aria2_lib PRIVATE ${LIBGMP_LIBRARIES})
endif()
if(HAVE_LIBGCRYPT)
  aria2_link_pkg(aria2_lib LIBGCRYPT)
endif()
if(HAVE_LIBSSH2)
  aria2_link_pkg(aria2_lib LIBSSH2)
endif()
if(HAVE_LIBNGHTTP2)
  aria2_link_pkg(aria2_lib LIBNGHTTP2)
endif()
if(HAVE_LIBCARES)
  aria2_link_pkg(aria2_lib LIBCARES)
endif()
if(HAVE_IOURING)
  aria2_link_pkg(aria2_lib LIBURING)
endif()
if(HAVE_TCMALLOC)
  aria2_link_pkg(aria2_lib TCMALLOC)
endif()
if(HAVE_JEMALLOC)
  aria2_link_pkg(aria2_lib JEMALLOC)
endif()
if(HAVE_WINTLS)
  target_link_libraries(aria2_lib PUBLIC ${WINTLS_LIBRARIES})
endif()
if(HAVE_APPLETLS)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
  find_library(SECURITY_FRAMEWORK Security)
  target_link_libraries(aria2_lib PUBLIC
    ${COREFOUNDATION_FRAMEWORK} ${SECURITY_FRAMEWORK})
endif()
if(ENABLE_WEBSOCKET)
  target_include_directories(aria2_lib PUBLIC ${WSLAY_INCLUDE_DIRS})
  target_link_libraries(aria2_lib PUBLIC wslay)
endif()

# gettext / intl
find_package(Intl)
if(Intl_FOUND)
  target_include_directories(aria2_lib PUBLIC ${Intl_INCLUDE_DIRS})
  target_link_libraries(aria2_lib PUBLIC ${Intl_LIBRARIES})
endif()

# ── aria2c executable ──────────────────────────────────────────────
add_executable(aria2c src/main.cc)
target_include_directories(aria2c PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/includes
  ${CMAKE_CURRENT_BINARY_DIR}/src/includes
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_definitions(aria2c PRIVATE
  LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
  HAVE_CONFIG_H
)
target_link_libraries(aria2c PRIVATE aria2_lib)

if(ARIA2_STATIC)
  if(NOT MINGW_BUILD AND NOT APPLE)
    target_link_libraries(aria2c PRIVATE pthread dl rt)
  endif()
  set_target_properties(aria2c PROPERTIES
    LINK_FLAGS "-static"
  )
endif()

# ── Tests ──────────────────────────────────────────────────────────
if(CPPUNIT_FOUND)
  enable_testing()

  set(TEST_SOURCES
    test/AllTest.cc
    test/TestUtil.cc
    test/SocketCoreTest.cc
    test/array_funTest.cc
    test/Base64Test.cc
    test/Base32Test.cc
    test/a2functionalTest.cc
    test/FileEntryTest.cc
    test/PieceTest.cc
    test/SegmentTest.cc
    test/GrowSegmentTest.cc
    test/SingleFileAllocationIteratorTest.cc
    test/DefaultBtProgressInfoFileTest.cc
    test/RequestGroupTest.cc
    test/UtilTest1.cc
    test/UtilTest2.cc
    test/UtilSecurityTest.cc
    test/UriListParserTest.cc
    test/HttpHeaderProcessorTest.cc
    test/RequestTest.cc
    test/HttpRequestTest.cc
    test/RequestGroupManTest.cc
    test/AuthConfigFactoryTest.cc
    test/NetrcAuthResolverTest.cc
    test/DefaultAuthResolverTest.cc
    test/OptionHandlerTest.cc
    test/SegmentManTest.cc
    test/BitfieldManTest.cc
    test/NetrcTest.cc
    test/SingletonHolderTest.cc
    test/HttpHeaderTest.cc
    test/HttpResponseTest.cc
    test/FileTest.cc
    test/OptionTest.cc
    test/DefaultDiskWriterTest.cc
    test/FeatureConfigTest.cc
    test/SpeedCalcTest.cc
    test/MultiDiskAdaptorTest.cc
    test/MultiFileAllocationIteratorTest.cc
    test/ProtocolDetectorTest.cc
    test/ExceptionTest.cc
    test/FmtTest.cc
    test/DownloadHandlersTest.cc
    test/SignatureTest.cc
    test/ServerStatManTest.cc
    test/FeedbackURISelectorTest.cc
    test/InorderURISelectorTest.cc
    test/ServerStatTest.cc
    test/NsCookieParserTest.cc
    test/DirectDiskAdaptorTest.cc
    test/CookieTest.cc
    test/CookieStorageTest.cc
    test/TimeTest.cc
    test/FtpConnectionTest.cc
    test/OptionParserTest.cc
    test/DNSCacheTest.cc
    test/DownloadHelperTest.cc
    test/SequentialPickerTest.cc
    test/RarestPieceSelectorTest.cc
    test/PieceStatManTest.cc
    test/LongestSequencePieceSelectorTest.cc
    test/a2algoTest.cc
    test/bitfieldTest.cc
    test/DownloadContextTest.cc
    test/SessionSerializerTest.cc
    test/ValueBaseTest.cc
    test/ChunkedDecodingStreamFilterTest.cc
    test/UriTest.cc
    test/UriSplitTest.cc
    test/CookieHelperTest.cc
    test/JsonTest.cc
    test/ValueBaseJsonParserTest.cc
    test/RpcResponseTest.cc
    test/RpcMethodTest.cc
    test/HttpServerTest.cc
    test/BufferedFileTest.cc
    test/GeomStreamPieceSelectorTest.cc
    test/SegListTest.cc
    test/ParamedStringTest.cc
    test/RpcHelperTest.cc
    test/AbstractCommandTest.cc
    test/SinkStreamFilterTest.cc
    test/WrDiskCacheTest.cc
    test/WrDiskCacheEntryTest.cc
    test/GroupIdTest.cc
    test/IndexedListTest.cc
    test/SimpleRandomizerTest.cc
    test/DownloadEngineTest.cc
    test/DownloadEngineMethodTest.cc
    test/SocketBufferTest.cc
    test/TimeBasedCommandTest.cc
    test/RealtimeCommandTest.cc
    test/HttpCommandIntegrationTest.cc
    test/FtpCommandIntegrationTest.cc
    test/SmallClassCoverageTest.cc
    test/SlowStartControllerTest.cc
    test/Socks5CommandTest.cc
    test/MessageDigestHelperTest.cc
    test/IteratableChunkChecksumValidatorTest.cc
    test/IteratableChecksumValidatorTest.cc
    test/MessageDigestTest.cc
  )

  # Conditional test sources
  if(ENABLE_XML_RPC)
    list(APPEND TEST_SOURCES test/XmlRpcRequestParserControllerTest.cc)
  endif()
  if(HAVE_SOME_FALLOCATE)
    list(APPEND TEST_SOURCES test/FallocFileAllocationIteratorTest.cc)
  endif()
  if(HAVE_ZLIB)
    list(APPEND TEST_SOURCES
      test/GZipDecoder.cc
      test/GZipDecoderTest.cc
      test/GZipEncoderTest.cc
      test/GZipDecodingStreamFilterTest.cc
      test/GZipFileTest.cc
    )
  endif()
  if(HAVE_SQLITE3)
    list(APPEND TEST_SOURCES test/Sqlite3CookieParserTest.cc)
  endif()
  if(ENABLE_BITTORRENT)
    list(APPEND TEST_SOURCES
      test/BtAllowedFastMessageTest.cc
      test/BtBitfieldMessageTest.cc
      test/BtCancelMessageTest.cc
      test/BtChokeMessageTest.cc
      test/BtHandshakeMessageTest.cc
      test/BtHaveAllMessageTest.cc
      test/BtHaveMessageTest.cc
      test/BtHaveNoneMessageTest.cc
      test/BtInterestedMessageTest.cc
      test/BtKeepAliveMessageTest.cc
      test/BtNotInterestedMessageTest.cc
      test/BtPieceMessageTest.cc
      test/BtPortMessageTest.cc
      test/BtRejectMessageTest.cc
      test/BtRequestMessageTest.cc
      test/BtSuggestPieceMessageTest.cc
      test/BtUnchokeMessageTest.cc
      test/DefaultPieceStorageTest.cc
      test/DefaultBtAnnounceTest.cc
      test/DefaultBtMessageDispatcherTest.cc
      test/DefaultBtRequestFactoryTest.cc
      test/AnnounceListTest.cc
      test/DefaultPeerStorageTest.cc
      test/ByteArrayDiskWriterTest.cc
      test/PeerTest.cc
      test/PeerSessionResourceTest.cc
      test/ShareRatioSeedCriteriaTest.cc
      test/BtRegistryTest.cc
      test/BtDependencyTest.cc
      test/BtPostDownloadHandlerTest.cc
      test/TimeSeedCriteriaTest.cc
      test/BtExtendedMessageTest.cc
      test/HandshakeExtensionMessageTest.cc
      test/UTPexExtensionMessageTest.cc
      test/UTMetadataRequestExtensionMessageTest.cc
      test/UTMetadataDataExtensionMessageTest.cc
      test/UTMetadataRejectExtensionMessageTest.cc
      test/UTMetadataRequestTrackerTest.cc
      test/UTMetadataRequestFactoryTest.cc
      test/UTMetadataPostDownloadHandlerTest.cc
      test/MagnetTest.cc
      test/DefaultBtMessageFactoryTest.cc
      test/DefaultExtensionMessageFactoryTest.cc
      test/DHTNodeTest.cc
      test/DHTBucketTest.cc
      test/DHTRoutingTableTest.cc
      test/DHTMessageTrackerEntryTest.cc
      test/DHTMessageTrackerTest.cc
      test/DHTConnectionImplTest.cc
      test/DHTPingMessageTest.cc
      test/DHTPingReplyMessageTest.cc
      test/DHTFindNodeMessageTest.cc
      test/DHTFindNodeReplyMessageTest.cc
      test/DHTGetPeersMessageTest.cc
      test/DHTGetPeersReplyMessageTest.cc
      test/DHTAnnouncePeerMessageTest.cc
      test/DHTAnnouncePeerReplyMessageTest.cc
      test/DHTUnknownMessageTest.cc
      test/DHTMessageFactoryImplTest.cc
      test/DHTBucketTreeTest.cc
      test/DHTPeerAnnounceEntryTest.cc
      test/DHTPeerAnnounceStorageTest.cc
      test/DHTTokenTrackerTest.cc
      test/XORCloserTest.cc
      test/DHTIDCloserTest.cc
      test/DHTRoutingTableSerializerTest.cc
      test/DHTRoutingTableDeserializerTest.cc
      test/DHTTaskExecutorTest.cc
      test/DHKeyExchangeTest.cc
      test/ARC4Test.cc
      test/MSEHandshakeTest.cc
      test/BittorrentHelperTest.cc
      test/PriorityPieceSelectorTest.cc
      test/LpdMessageDispatcherTest.cc
      test/LpdMessageReceiverTest.cc
      test/Bencode2Test.cc
      test/PeerConnectionTest.cc
      test/ValueBaseBencodeParserTest.cc
      test/ExtensionMessageRegistryTest.cc
      test/UDPTrackerClientTest.cc
      test/BtOrchestrationTest.cc
      test/DHTCommandTest.cc
      test/BtPeerIntegrationTest.cc
      test/BtValidatorTest.cc
      test/StreamPieceSelectorTest.cc
      test/DHTMessageDispatchTest.cc
      test/DHTTaskTest.cc
    )
  endif()

  if(ENABLE_METALINK)
    list(APPEND TEST_SOURCES
      test/MetalinkerTest.cc
      test/MetalinkEntryTest.cc
      test/Metalink2RequestGroupTest.cc
      test/MetalinkPostDownloadHandlerTest.cc
      test/MetalinkHelperTest.cc
      test/MetalinkParserControllerTest.cc
      test/MetalinkProcessorTest.cc
    )
  endif()

  if(ENABLE_ASYNC_DNS)
    list(APPEND TEST_SOURCES test/AsyncNameResolverTest.cc)
  endif()

  if(NOT HAVE_TIMEGM)
    list(APPEND TEST_SOURCES test/TimegmTest.cc)
  endif()

  if(ENABLE_LIBARIA2)
    list(APPEND TEST_SOURCES test/Aria2ApiTest.cc)
  endif()

  add_executable(aria2_test ${TEST_SOURCES})
  target_include_directories(aria2_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/includes
    ${CMAKE_CURRENT_BINARY_DIR}/src/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CPPUNIT_INCLUDE_DIRS}
  )
  target_compile_definitions(aria2_test PRIVATE
    HAVE_CONFIG_H
    A2_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test"
    A2_TEST_OUT_DIR="test_outdir"
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
  )
  target_link_libraries(aria2_test PRIVATE
    aria2_lib
    ${CPPUNIT_LIBRARIES}
  )
  if(CPPUNIT_LIBRARY_DIRS)
    target_link_directories(aria2_test PRIVATE ${CPPUNIT_LIBRARY_DIRS})
  endif()

  add_test(NAME aria2_test COMMAND aria2_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()

# ── Install rules ──────────────────────────────────────────────────
install(TARGETS aria2c
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(ENABLE_LIBARIA2)
  install(TARGETS aria2_lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(FILES src/includes/aria2/aria2.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aria2)

  # pkg-config file
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libaria2.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libaria2.pc"
    @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libaria2.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# ── Build summary ─────────────────────────────────────────────────
message(STATUS "")
message(STATUS "  aria2 ${PROJECT_VERSION} build configuration:")
message(STATUS "")
message(STATUS "  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler:    ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "  SSL Support:     ${HAVE_SSL}")
message(STATUS "    AppleTLS:      ${HAVE_APPLETLS}")
message(STATUS "    WinTLS:        ${HAVE_WINTLS}")
message(STATUS "    GnuTLS:        ${HAVE_LIBGNUTLS}")
message(STATUS "    OpenSSL:       ${HAVE_OPENSSL}")
message(STATUS "  Zlib:            ${HAVE_ZLIB}")
message(STATUS "  SQLite3:         ${HAVE_SQLITE3}")
message(STATUS "  LibXML2:         ${HAVE_LIBXML2}")
message(STATUS "  LibExpat:        ${HAVE_LIBEXPAT}")
message(STATUS "  LibCares:        ${HAVE_LIBCARES}")
message(STATUS "  Libssh2:         ${HAVE_LIBSSH2}")
message(STATUS "  Libnghttp2:      ${HAVE_LIBNGHTTP2}")
message(STATUS "  LibUV:           ${HAVE_LIBUV}")
message(STATUS "  LibNettle:       ${HAVE_LIBNETTLE}")
message(STATUS "  LibGmp:          ${HAVE_LIBGMP}")
message(STATUS "  LibGcrypt:       ${HAVE_LIBGCRYPT}")
message(STATUS "  Tcmalloc:        ${HAVE_TCMALLOC}")
message(STATUS "  Jemalloc:        ${HAVE_JEMALLOC}")
message(STATUS "")
message(STATUS "  BitTorrent:      ${ENABLE_BITTORRENT}")
message(STATUS "  Metalink:        ${ENABLE_METALINK}")
message(STATUS "  WebSocket:       ${ENABLE_WEBSOCKET}")
message(STATUS "  XML-RPC:         ${ENABLE_XML_RPC}")
message(STATUS "  Async DNS:       ${ENABLE_ASYNC_DNS}")
message(STATUS "  Epoll:           ${HAVE_EPOLL}")
message(STATUS "  io_uring:        ${HAVE_IOURING}")
message(STATUS "  Kqueue:          ${HAVE_KQUEUE}")
message(STATUS "  Libaria2:        ${ENABLE_LIBARIA2}")
message(STATUS "  Static build:    ${ARIA2_STATIC}")
message(STATUS "")
