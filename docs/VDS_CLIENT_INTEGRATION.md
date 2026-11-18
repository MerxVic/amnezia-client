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

The panel must enforce these permissions on every request. Client accounts
are bound to one profile and must be independently revocable. The application
does not store the panel password; it keeps the session cookie in memory and
never places credentials in a URL, log, crash report, or VPN profile export.

## Connection types

AWG is the primary connection. VLESS Reality is a separate client connection,
not a fallback transport inside AWG. Existing server-to-server QUIC routes do
not expose a mobile QUIC endpoint. Client-facing QUIC is reserved for a future
extension; it is outside the current delivery scope. Its engine and matching
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
The current server list also contains `*.domain` entries. The upstream
IP-route mechanism cannot cover every subdomain from such a rule. This fork
skips those entries with a visible warning while applying exact domains and
IPv4 entries; domain-aware matching remains required for full wildcard support.

The first parser is `IpSplitTunnelingController::importManagedBypassPolicy`.
It validates a bounded JSON response and maps it to the upstream direct-route
mode. The settings screen can import a downloaded file or fetch the public
list directly from a supplied HTTPS panel URL. After the first successful
download, the app checks that URL hourly and applies new rules when the VPN is
disconnected. At this stage, each accepted update replaces the local direct
route list; separate managed/manual storage remains to be implemented. The
public list does not require a device token.

The connection-key screen now accepts a panel URL and client account. It
downloads the account's own `.vpn` profile into the existing import review
screen. The password is not persisted; the session cookie remains in memory.
After login, the home screen shows a reduced AWG/VLESS availability summary
from `/api/client/status`, refreshed while the application is running.
Client sign-in also starts the direct-route list download from that panel's
public HTTPS endpoint; later refreshes use the saved URL.
Client mode shows the connection button and basic availability; it hides the
settings, sharing, add-profile, server, and split-tunneling controls. These UI
restrictions supplement the panel's server-side authorization. Administrator
accounts use the existing panel login and `/api/config` role check. The app
offers a link to the full administration panel; the system browser has its own
session and requires a separate login. Native administration controls remain
to be implemented. The last selected UI role is stored locally so that a
client device still opens in its simple mode after an app restart; the panel
rechecks access for every authenticated request.

## Delivery gates

1. Add panel endpoints for device enrollment, own-profile download, and
   client-safe status. Test role enforcement and revocation.
2. Wire policy download and refresh into the client. Preserve
   manual rules separately from the managed list. Apply updates only after the
   active VPN tunnel can safely reload routes or during controlled reconnect.
3. Add the two role-specific interfaces, including Android TV remote navigation.
4. Verify AWG and VLESS on real devices.
5. Build and sign Android and iOS artifacts with the owner's signing accounts.

The upstream project is GPL-3.0; distributed modified clients must retain the
required source and license notices.
