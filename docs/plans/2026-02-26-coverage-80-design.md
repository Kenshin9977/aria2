# Coverage 80% — Design

Date: 2026-02-26

## Problem

Test coverage is 61.9% lines (22 551 / 36 407). 69 source files at 0%.
Major gaps: download pipelines, connection init, DHT, proxy, MSE.
TLS/SSH/SFTP excluded (platform-specific, ~500L).

## Target

80% line coverage (~29 125 lines covered, +6 574 from current).

## Strategy: 3-Tier Approach

### Tier 1 — Quick Wins (~500L, gets to ~65%)

Unit tests for small/trivial 0% files:
- BT validators: BtBitfieldMessageValidator, BtHandshakeMessageValidator,
  BtPieceMessageValidator, IndexBtMessageValidator
- Piece selectors: InorderStreamPieceSelector, RandomStreamPieceSelector
- Small classes: UnionSeedCriteria, ContextAttribute, Notifier,
  FatalException, TruncFileAllocationIterator, UnknownOptionException,
  DHTMessageEntry, NameResolver, XmlRpcDiskWriter
- Partial coverage boost: CheckIntegrityEntry, BtPieceMessage, PeerStat

Single test file: `test/QuickWinCoverageTest.cc`

### Tier 2 — Pipeline Integration (~2 500L, gets to ~72%)

Extend existing integration tests to cover full download pipelines:

**HTTP full download:**
- 200 OK with body download through DownloadCommand
- 301 redirect handling
- Covers: HttpInitiateConnectionCommand, ConnectCommand,
  HttpDownloadCommand, DownloadCommand, HttpSkipResponseCommand

**FTP PASV download:**
- Full negotiation: greeting→USER→PASS→TYPE→PWD→SIZE→EPSV→RETR
- Covers: FtpInitiateConnectionCommand, FtpDownloadCommand,
  FtpFinishDownloadCommand

**BT extended:**
- Bitfield exchange post-handshake
- Covers: ActivePeerConnectionCommand, PeerReceiveHandshakeCommand

**New file:** `test/InitiateConnectionIntegrationTest.cc` — tests
InitiateConnectionCommand → protocol-specific dispatch.

### Tier 3 — Component Integration (~3 000L, gets to ~80%)

**DHT:** `test/DHTIntegrationTest.cc`
- Find node round-trip, ping, announce peer
- Covers: DHTSetup, DHTInteractionCommand, DHTMessageReceiver,
  DHTMessageDispatcherImpl, DHTTaskFactoryImpl, DHTTaskQueueImpl,
  DHTAbstractTask, DHTPingTask, DHTBucketRefreshTask,
  DHTNodeLookupTask, DHTPeerLookupTask + callbacks

**Proxy:** `test/ProxyIntegrationTest.cc`
- HTTP CONNECT tunnel, FTP tunnel
- Covers: AbstractProxyRequest/ResponseCommand,
  HttpProxyRequest/ResponseCommand, FtpTunnelRequest/ResponseCommand

**MSE:** `test/MSEIntegrationTest.cc`
- Initiator + receiver handshake
- Covers: InitiatorMSEHandshakeCommand, ReceiverMSEHandshakeCommand

**BT setup:** `test/BtSetupTest.cc`
- Bootstrap setup flow
- Covers: BtSetup, BtCheckIntegrityEntry, BtFileAllocationEntry

**Branch coverage boost** on partially covered files:
- AbstractCommand (33→70%), RequestGroup (38→70%),
  RequestGroupMan (36→70%), DefaultBtInteractive (17→60%)

## Test Architecture

All integration tests use the Phase 3 pattern:
- Real SocketCore pairs via listen/connect/accept
- Real DownloadEngine with SelectEventPoll
- engine->addCommand() + engine->run(true) for deterministic ticks
- TestHaltCommand for engine drain
- Single-threaded, no flakiness

## Files Changed

| Action | File |
|--------|------|
| Create | `test/QuickWinCoverageTest.cc` |
| Create | `test/InitiateConnectionIntegrationTest.cc` |
| Create | `test/DHTIntegrationTest.cc` |
| Create | `test/ProxyIntegrationTest.cc` |
| Create | `test/MSEIntegrationTest.cc` |
| Create | `test/BtSetupTest.cc` |
| Modify | `test/HttpCommandIntegrationTest.cc` |
| Modify | `test/FtpCommandIntegrationTest.cc` |
| Modify | `test/BtPeerIntegrationTest.cc` |
| Modify | `test/Makefile.am` |
| Create | `scripts/coverage.sh` |
| Modify | `.gitignore` |

Production code changes: ISocketCore interface extraction (65 files in src/).
SocketCore now implements the ISocketCore abstract interface. All Command classes
use shared_ptr<ISocketCore> instead of shared_ptr<SocketCore>. This enables
MockSocketCore in tests. Code needing concrete SocketCore (TLS, SSH, socket pool)
uses static_pointer_cast.

## Exclusions

- AppleTLSSession.cc (300L) — platform-specific
- SSHSession.cc (~100L) — requires libssh2
- Sftp*.cc (~100L) — requires libssh2
- AsyncNameResolver.cc — requires c-ares runtime

## Success Criteria

- `lcov --summary` shows ≥80% line coverage
- All tests pass: `make check` → OK, EXIT 0
- Coverage script: `./scripts/coverage.sh` works end-to-end
