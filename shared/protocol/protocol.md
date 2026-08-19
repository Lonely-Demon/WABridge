# WABridge Protocol 1

## Transport

Application data is carried only after TLS 1.3 establishment and pinned-peer verification. The protocol does not define a cryptographic primitive. Discovery, QR data, and manual endpoints identify candidates; they never authenticate a peer.

## Envelope

All fields are network byte order:

```text
magic       u16   0x5742
version     u8    1
channel     u8    1=control, 2=media, 3=file, 4=clipboard, 5=audio
kind        u16   message kind
flags       u16   request/response/stream/end/error
request_id  u64   non-zero for request/response pairs
length      u32   bounded payload length
payload     N     canonical JSON or binary bytes
```

A receiver must reject a bad magic value, unsupported version, unknown channel, invalid flags, zero or excessive length, duplicate request IDs, and any frame that exceeds both the channel limit and the session budget. It must not allocate based on unchecked remote input.

## Initial control messages

```text
0x0001 SESSION_HELLO
0x0002 CAPABILITIES
0x0003 PAIRING_STATUS
0x0004 SAS_CONFIRMATION
0x0005 HEARTBEAT
0x0006 HEARTBEAT_ACK
0x0007 SESSION_CLOSE
0x0008 ERROR
```

`SESSION_HELLO` contains protocol major version, role, fresh session nonce, stable non-secret device ID, capability hash, and maximum frame size. `PAIRING_STATUS` contains the peer fingerprint and human-comparison code but no private material. `CAPABILITIES` is exchanged only after the TLS identity and application session are authenticated.

## Channel limits

| Channel | Single frame | Queue budget |
|---|---:|---:|
| Control | 64 KiB | 256 KiB |
| Clipboard | 1 MiB | 4 MiB |
| File | 1 MiB | 16 MiB |
| Media | 4 MiB | 32 MiB |
| Audio | 256 KiB | 4 MiB |

## Session states

```text
Idle -> Discovering -> Connecting -> TlsHandshaking ->
IdentityChecking -> PairingRequired -> Established -> Closing -> Idle
                     |                  |
                     +-> Failed <-------+
```

`stop()` is legal in every state, is idempotent, cancels pending I/O, closes TLS, releases queues, and emits exactly one terminal state transition.
