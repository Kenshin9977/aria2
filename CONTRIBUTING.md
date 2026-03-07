# Contributing to aria2

Thank you for considering contributing to aria2!

## Building from source

### Prerequisites

aria2 uses Autotools. Install build dependencies for your platform:

**Linux (Debian/Ubuntu):**

```bash
sudo apt-get install \
  g++ autoconf automake autotools-dev autopoint libtool pkg-config \
  libssl-dev libc-ares-dev zlib1g-dev libsqlite3-dev libssh2-1-dev \
  libcppunit-dev libxml2-dev
```

**macOS (Homebrew):**

```bash
brew install autoconf automake libtool pkgconf openssl@3 c-ares \
  libssh2 sqlite libxml2 cppunit
```

### Build

```bash
autoreconf -i
./configure
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### Run tests

```bash
make check
```

Tests use CppUnit. Test files are in `test/` and follow the pattern
`test/<ClassName>Test.cc`.

## Code style

The project uses clang-format. The `.clang-format` file in the repo root
is the source of truth. Key rules:

- 2-space indentation, no tabs
- 80 column limit
- Stroustrup brace style
- Left-aligned pointers: `int* ptr`
- C++11 standard (no C++14/17/20 features)

Format your changes:

```bash
make clang-format
```

## Commit messages

Follow Conventional Commits (Commitizen format):

```
<type>(<scope>): <short description>
```

Types: `feat`, `fix`, `test`, `refactor`, `ci`, `docs`, `chore`, `style`

Examples:

```
feat(rpc): add batch download support
fix(bt): handle tracker timeout correctly
test(dht): add DHT routing table tests
```

## Git workflow

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-change`
3. Make atomic commits (one logical change per commit)
4. Rebase on master before submitting: `git rebase master`
5. Open a Pull Request

## Architecture overview

- Entry point: `src/main.cc`
- Download engine: `src/DownloadEngine.cc` (single-threaded event loop)
- Protocol handlers: `src/Http*.cc`, `src/Ftp*.cc`, `src/Bt*.cc`
- RPC interface: `src/Rpc*.cc`, `src/Json*.cc`, `src/Xml*.cc`
- Public API: `src/includes/aria2/aria2.h`
- Tests: `test/`

## Reporting issues

Open an issue at https://github.com/Kenshin9977/aria2/issues with:

- aria2 version (`aria2c --version`)
- Operating system
- Steps to reproduce
- Expected vs actual behavior
