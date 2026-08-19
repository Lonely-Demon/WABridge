# WABridge

WABridge is a clean-slate Windows–Android local workspace bridge. It is designed for Windows 10/11 and Android over Wi-Fi, with no Bluetooth, USB, cloud account, or Internet dependency for core features.

The project is intentionally separate from BetterCast. BetterCast remains a reference for requirements, regression cases, and lessons learned; its implementation is not reused as the new runtime foundation.

## Product modes

WABridge has one authenticated device session with independent feature modules:

- **Workspace mode:** use an Android phone as a Windows extended display when the signed Windows IDD component is available.
- **Phone mode:** keep the normal Android UI visible and use explicitly authorized Windows pointer/keyboard control.
- **Bridge modules:** add file transfer, clipboard, notifications, audio, and optional mirroring without coupling their lifecycles to display streaming.

## Current vertical slice

The first prototype foundation defines the shared protocol envelope, versioned capability exchange, session state machine, platform identity boundaries, and module interfaces. No feature is considered complete until its start/stop path is idempotent and its failure is isolated from the authenticated control session.

## Repository layout

```text
shared/protocol/   Versioned protocol constants, schemas, and test vectors
windows/           Windows coordinator, transport, pairing, and future IDD integration
android/           Android companion, transport, pairing, and future services
tests/             Cross-platform protocol and lifecycle tests
docs/              Architecture and threat-model records
```

## Security baseline

The protocol uses TLS 1.3 rather than application-implemented cryptography. Device certificates are pinned after first-pair SAS comparison. Discovery is candidate-only and never grants trust. No plaintext application frames are accepted. Every parser is bounded, every module has independent queues, and every session shutdown is idempotent.

## Build status

The current scaffold is specification-first. Windows CI will provide the authoritative native build because the sandbox does not contain the Windows SDK, Qt, or Android SDK. The first implementation milestone is a TLS 1.3 loopback/local-network pairing slice before display-driver or MediaProjection work begins.
