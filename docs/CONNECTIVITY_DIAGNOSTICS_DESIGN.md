# WABridge Connectivity, Diagnostics, and QR Pairing Design

## Goals

WABridge must make a failed connection explainable without requiring the user to guess whether discovery, TCP reachability, TLS verification, or first-pair approval failed. The Windows client should advertise its availability on the local Wi-Fi network while it is listening, and Android should provide both a BetterCast-style connection log and a QR-assisted bootstrap path.

## Local availability architecture

The Windows coordinator remains the authoritative local service. When the desktop client starts, it binds its configured TCP listener, enumerates non-loopback local addresses, and publishes `_wabridge._tcp.local` through a small mDNS responder. The responder sends unsolicited announcements and answers discovery queries. Each record includes the active TCP port, Windows device ID, Protocol 1 version, and a short-lived pairing hint. It must stop when the coordinator stops.

The availability service does not create a cloud account, open a public relay, or accept unauthenticated feature traffic. TCP remains protected by TLS 1.3 and bilateral certificate pinning. Windows Firewall remains a separate host-policy boundary; the UI must report when the listener is active and when the multicast socket cannot bind, rather than claiming that the laptop is discoverable.

## QR bootstrap payload

The QR code carries a versioned, bounded, non-secret payload such as:

`wabridge://pair?v=1&host=192.168.x.x&port=51820&device_id=DESKTOP&fp=SHA256_PUBLIC_KEY_FINGERPRINT&expires=...&nonce=...`

The payload contains an endpoint and the Windows certificate public-key fingerprint so Android can connect to the intended device and reject a certificate mismatch. It never contains a private key, password, session secret, or a reusable authentication token. The payload expires quickly and is regenerated for each pairing display. A QR scan only fills the endpoint and expected fingerprint; the existing TLS first-pair approval remains mandatory.

If a QR payload is copied or photographed, it can at most reveal the local endpoint and certificate identity during its short validity window. It cannot authenticate an attacker or decrypt a session.

## Diagnostics model

Both clients use bounded in-memory logs. Entries include a timestamp, severity, and sanitized message. Logs cover service startup, listener binding, mDNS bind/announce/query handling, endpoint selection, TCP connect, TLS handshake, certificate/pairing decision, session establishment, and teardown. Private keys and raw network credentials are never logged.

Android exposes these entries in a dedicated Logs tab. Windows exposes them in a dedicated Logs page and includes the selected port, advertised addresses, mDNS state, firewall caveat, and last connection result.

## Implementation order

1. Add structured logs and logs pages.
2. Upgrade the Windows mDNS announcer into a query-responsive availability service and report its state.
3. Add QR payload generation and display on Windows.
4. Add Android QR scanning and strict payload validation.
5. Add codec/unit tests for bounds, expiry, fingerprint syntax, host/port validation, and rejection of private-key-like fields.
