# VDS client integration

This fork targets iOS, Android, and Android TV. It retains the upstream VPN
engines and adds a separate, optional integration with a self-hosted VDS panel.
No server address, profile, access token, or private key belongs in this repo.

## Roles

| Capability | Client | Administrator |
| --- | --- | --- |
| Connect and disconnect AWG or VLESS Reality | Own profile | Assigned profiles |
| Import profile and select protocol | Own profile | Yes |
| See basic VPN and server availability | Yes | Yes |
| Read the managed direct-route list | Yes | Yes |
| Edit the direct-route list | No | Yes |
| Create, rotate, revoke, or export other profiles | No | Yes |
| Change server routes or restart a server | No | Yes, with a separate confirmation |

The panel must enforce these permissions on every request. Client tokens must
be bound to one device/profile and be independently revocable. The application
stores credentials in platform protected storage and never places them in a
URL, log, crash report, or VPN profile export.

## Connection types

AWG is the primary connection. VLESS Reality is a separate client connection,
not a fallback transport inside AWG. Existing server-to-server QUIC routes do
not expose a mobile QUIC endpoint. A client-facing QUIC engine and its matching
server entry must be chosen and tested before enabling this protocol in the UI.
Each connection type must be tested on mobile data and Wi-Fi, including DNS
failover, reconnect, tunnel shutdown, and visible exit IP.

## Managed direct routes

The existing panel publishes an Amnezia-compatible JSON document over HTTPS
at `/downloads/ru-bypass-ip-list.json` on the primary, backup, and emergency
panel addresses. It is refreshed on the server once per hour:

```json
[{"hostname":"example.org","ip":""},{"hostname":"192.0.2.0/24","ip":""}]
```

These entries mean **bypass the VPN on the device**. They are independent of
the panel's current `/api/routing/rules`, which controls routing on Russian
entry servers and has different semantics. The application validates the
entire response, keeps the last accepted version on failed refresh, and applies
new rules before the next connection. An empty list is valid. Domain entries
must be resolved again when their IP addresses change; shared IP addresses
can affect unrelated domains. IPv6 needs a separate verified implementation.

The first parser is `IpSplitTunnelingController::importManagedBypassPolicy`.
It validates a bounded JSON response and maps it to the upstream direct-route
mode. The settings screen can import a downloaded file or fetch the public
list directly from a supplied HTTPS panel URL. Authenticated profile
provisioning, periodic refresh, and separate managed/manual storage remain to
be implemented. The public list does not require a device token.

## Delivery gates

1. Add panel endpoints for device enrollment, own-profile download, and
   client-safe status. Test role enforcement and revocation.
2. Wire policy download and refresh into the client. Preserve
   manual rules separately from the managed list. Apply updates only after the
   active VPN tunnel can safely reload routes or during controlled reconnect.
3. Add the two role-specific interfaces, including Android TV remote navigation.
4. Verify AWG and VLESS on real devices. Add mobile QUIC only after a matching
   client-facing server endpoint passes end-to-end tests.
5. Build and sign Android and iOS artifacts with the owner's signing accounts.

The upstream project is GPL-3.0; distributed modified clients must retain the
required source and license notices.
