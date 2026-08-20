# WABridge UI Redesign Specification

## Direction

WABridge should feel like a polished companion product rather than a developer control panel. The design should borrow the original BetterCast visual language—dark premium surfaces, strong blue/cyan identity, rounded glass cards, soft depth, large hierarchy-driven headings, compact status badges, gradient primary actions, and floating navigation—while remaining a clean Windows–Android product.

The redesign must not copy BetterCast platform references, unsupported performance claims, or old product flows. WABridge is Wi-Fi-only, Windows-first, Android-only, and its primary capabilities are Second Display, Phone Control, file transfer, clipboard, audio routing, notifications, and suspend/resume.

## Shared tokens

| Token | Value / behavior |
|---|---|
| Base background | Near-black charcoal in dark mode; restrained cool gray in light mode. |
| Primary accent | Electric blue, used for the WABridge mark, selected navigation, primary actions, and healthy connection emphasis. |
| Secondary accent | Cyan, used to complete gradients and distinguish audio/display capability highlights. |
| Attention accents | Gold for pairing approval and permission requests; orange for warnings; green for connected/ready; red for destructive disconnect/error. |
| Surfaces | Rounded 18–24 dp cards on Android and 12–16 px cards on Windows, with translucent or slightly lifted charcoal fill, hairline border, and restrained shadow. |
| Primary action | Full-width or prominent blue-to-cyan gradient action with white semibold text. |
| Secondary action | Dark glass/outlined action, never visually competing with the primary action. |
| Type hierarchy | Large bold screen title, medium card title, readable body copy, small uppercase/letter-spaced badges. |
| Navigation | Android floating bottom pill with icon-led tabs; Windows left rail or top-level capability cards with the active feature clearly highlighted. |

## Android information architecture

The Android app should open to a branded Connect dashboard. The top area contains the WABridge mark, current state badge, and a concise state explanation. The main hero card presents the active Windows laptop or discovery state. Discovery is the primary action; manual endpoint entry is a secondary expandable card, not the first visual object. Pairing approval appears as a prominent gold glass card with device name and fingerprint. Capability cards show Second Display, Phone Control, Files, Clipboard, and Audio with availability/permission state. A floating pill switches between Connect, Features, Setup, and Settings.

The implementation must keep explicit permission gates visible. MediaProjection and AccessibilityService are shown as user-authorized prerequisites, never implied to be automatic. Disabled capabilities should explain why they are disabled instead of appearing as unexplained gray buttons.

## Windows information architecture

The Windows app should open as a polished coordinator dashboard rather than a raw form. A compact branded header contains the WABridge mark, connection state, and window actions. A left navigation rail contains Dashboard, Phone Control, Audio, Files, Clipboard, and Settings. The Dashboard presents a large connection card with discovery status, port, pairing state, and primary Start/Stop action. A capability grid presents Phone Control, Android Audio, Second Display, and File Transfer as glass cards with status badges. Advanced certificate override paths and port configuration belong in Settings/Advanced, not the first screen.

Phone Control and Audio cards must expose clear readiness state: session disconnected, awaiting pairing, permission required, active, or unavailable. The existing secure status text remains available in a compact activity/diagnostics panel rather than dominating the dashboard.

## Security and state language

Use concrete, calm language: “Waiting for Android approval”, “Encrypted session ready”, “Accessibility access required”, “MediaProjection permission required”, and “No Windows laptop discovered”. Avoid claiming that pinning makes compromise impossible. Pairing fingerprints should be visually prominent but never truncated in a way that prevents comparison.

## Acceptance criteria

The redesign is complete only when both clients visibly share the same palette, card language, state hierarchy, and action emphasis; BetterCast/Mac/iOS terminology is absent from WABridge UI; existing session callbacks and permission launchers remain connected; Android JVM tests and Windows/native CI remain green; and rendered previews show a substantial improvement over the current plain-form UI.
