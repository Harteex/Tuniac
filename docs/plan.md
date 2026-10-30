# Jellyfin / service plugins — plan

Working plan for the `jellyfin` branch. Tick boxes as things land. Background and
line-level detail: [service-plugins.md](service-plugins.md) (referenced below as §n).

Status: **plugin type loads, nothing works end to end.** Login code exists but has no
caller and a body-encoding bug; no audio plays; nothing reaches the library.

---

## Phase 0 — Clear the ground (small, do first)

Cheap fixes that would otherwise bite during the later phases.

- [x] `PrefPage m_Pages[11]` → `[12]` in `Preferences.h` (§5.2). Currently overwrites
      memory and hides the EQ page.
- [x] Version check: export/look up `GetTuniacServicePluginVersion` and compare against
      `ITUNIACSERVICEPLUGIN_VERSION` (§5.3). Today the check uses the *general* plugin
      constant, so an upstream bump of `ITUNIACPLUGIN_VERSION` would reject the Jellyfin
      plugin even though its interface didn't change.
- [x] `ServicesProc`: use `SERVICEPLUGINFLAGS_*`, not `PLUGINFLAGS_*` (§5.4).
- [x] Decide the registry home now, before tokens get stored: move service plugins to
      `HKCU\Software\Tuniac\services\…` (§5.5). Moving later means migrating saved logins. Done: enable flags under
      `services`, plugin settings under `services\<name>`.
- [x] Store the plugin's `HINSTANCE` from `DllMain` instead of
      `GetModuleHandle(L"Jellyfin_Plugin.dll")` (§5.7).

## Phase 1 — Authentication

Goal: given server URL + username + password, end up with a persisted token and user
id, and be able to tell on startup whether we are still logged in.

- [x] **Fix the POST body** (§5.1): `CHttpService::Post` takes `const char*` + byte
      length; `Login()` sends RapidJSON's UTF-8 buffer directly, no UTF-16 round trip.
- [x] **Send the client authorization header on every request, including login.**
      `CJellyfinService::BuildAuthHeader` — `Authorization: MediaBrowser Client="Tuniac",
      Device="…", DeviceId="…", Version="…"[, Token="…"]`, device name percent-encoded.
- [x] **Stable DeviceId**: GUID generated once, stored as `services\Jellyfin\DeviceId`.
- [x] **Parse and keep more than the token**: `AccessToken`, `User.Id`, `User.Name`,
      `ServerId`.
- [x] **Persist** `Url`, `UserId`, `UserName`, `Token` under
      `services\Jellyfin\Servers\<ServerId>`, plus `services\Jellyfin\ActiveServer`.
      Password is never stored. Logging in to another server keeps the first one's token.
- [x] **Validate**: `ValidateSession()` does `GET /Users/Me`; 401/403 clears the token
      locally, a network failure leaves it alone. Changed from "on startup" to **on
      demand** so an unreachable server can't stall Tuniac's startup — call it when the
      Configure dialog opens (Phase 2) and before playback (Phase 3).
- [x] **Logout**: `POST /Sessions/Logout`, then clear the token locally regardless.
      Server URL and user name are kept to prefill the dialog.
- [x] **Error reporting**: `JellyfinResult` (`INVALIDARGS` / `CONNECT` / `CERTIFICATE` /
      `UNAUTHORIZED` / `RESPONSE`), a readable `GetLastErrorText()` for the UI, and every
      failure logged via `LogMessage`.
- [x] **TLS**: ignore-cert flags removed. Opt-in `AllowInvalidCertificates` setting
      (`SetAllowInvalidCertificates()`), which also accepts unknown CAs — the old flags never
      did, so self-signed servers never actually worked. One setting for the plugin for now,
      not per server; Phase 2 needs a checkbox for it.
- [x] Requests have timeouts (10 s connect, 30 s send/receive), since they run on the UI
      thread.
- [x] Host fix: `CPreferences::PluginGetValue` now returns false when the value is
      missing (§5.7), so "is there a stored token?" can be answered.

Tested with a throwaway console harness against `demo.jellyfin.org/stable`: login (with a
trailing slash in the URL), wrong password, validate, logout, and validate with a token
the server had revoked. Also: missing URL scheme and unreachable host. The certificate
path is untested.

Decided: login runs on the UI thread, bounded by the request timeouts. Revisit if it
feels sluggish.

## Phase 2 — Plugin configuration UI

Goal: the Configure dialog is the login/logout screen.

- [x] Replace the PopupNotify-cloned dialog with `IDD_JELLYFIN_CONFIG`: Server URL,
      Username, Password, **Test connection** / **Log in** / **Log out**, a Status box
      ("Logged in as X on Y" / "Not logged in" plus the last result or error), Close.
- [x] Logged in: URL and username read-only, password disabled, only Log out enabled.
      Logged out: the reverse, with URL and username prefilled from settings.
- [x] Opening the dialog while logged in checks the stored login (`ValidateSession`),
      posted after the dialog is shown so it doesn't delay opening.
- [x] "Allow untrusted HTTPS certificates (self-signed)" checkbox, saved immediately.
- [x] Normalise the server URL (trim whitespace and trailing `/`, require `http://` or
      `https://`) — `NormalizeServerUrl`, shared by Login and Test connection.
- [x] Remove the commented-out PopupNotify code from `DlgProc` and the header, rename
      leftovers. `DlgProcStub` no longer calls through a NULL pointer for messages that
      arrive before `WM_INITDIALOG` (§5.7).
- [x] "Test connection" hits `/System/Info/Public` (no auth) and shows the server name and
      version; a 404 says it doesn't look like a Jellyfin server.

Requests block the dialog while they run (status line shows what's happening, wait
cursor). Verified by driving the real DLL's dialog against `demo.jellyfin.org/stable`
and capturing it in each state. Not yet seen from inside Tuniac's Preferences.

## Phase 3 — Playback of stored `jellyfin://` links

**Does it work already? No.** The only code that ever played Jellyfin audio is the
prototype `CJellyfinService` in `TuniacApp/`, and its call in `CCoreAudio::SetSource`
(`CoreAudio.cpp:253`) is commented out. It also has a latent bug if simply uncommented:
`szNewSource` is declared inside the `if` block, so `szSource` dangles once the block
exits.

**URL format (decided):** `jellyfin://<serverid>/<itemid>`. Multiple servers are allowed;
the plugin looks up host and token by server id when translating.

**`jellyfin://` is a Tuniac-internal scheme, not something Windows knows about.** Don't
rely on `PathIsURL` or other shell URL handling for it. The core uses `PathIsURL` to
decide "stream vs. file" in several places (`CMediaLibrary::AddItem`, `BasePlaylist.cpp`,
`LibraryPlaylist.cpp`, `ImportExportManager.cpp`, and `bass_Plugin`'s `CanHandle`), so:

- [ ] Add a host-side check, e.g. `CServicePluginManager::CanHandle(szSource)`, and use it
      next to `PathIsURL` wherever service links must be treated as streams (at minimum
      `AddItem`, so they become `ENTRY_KIND_URL` entries instead of failing as files).
- [ ] Audit the `PathIsURL` call sites in `BasePlaylist.cpp` / `LibraryPlaylist.cpp` and
      decide per site whether service links should count as URLs.
- [ ] Never hand a raw `jellyfin://` string to a decoder — it must be translated first.

Quick check worth doing first: temporarily re-enable the hook (with the buffer hoisted
to function scope), put Host/Token in the registry, and confirm a translated stream URL
plays through BASS. That tells us the "URL + `\r\n` + headers" path still works before we
move it. (Getting a `jellyfin://` entry into the library for the test may need the
`AddItem` change above, or a hand-made entry.)

Then do it properly through the plugin:

- [ ] Add to `ITuniacServicePlugin`: `bool CanHandle(LPTSTR szSource)` and
      `bool TranslateSource(LPTSTR szDest, size_t cchDest, LPTSTR szSource)`. No version
      bump: the interface is unreleased and the only plugin is built with the app. Start
      bumping `ITUNIACSERVICEPLUGIN_VERSION` on interface changes once plugins ship
      separately.
- [ ] Add `CServicePluginManager::TranslateSource(...)` that walks enabled plugins.
- [ ] `CCoreAudio::SetSource`: call it before the decoder loop, buffer at function scope.
- [ ] Move `TranslateSource` logic into `Jellyfin_Plugin`, using the Phase 1 settings
      and auth-header helper.
- [ ] Delete `TuniacApp/JellyfinService.{h,cpp}`, its construction in
      `CCoreAudio::Startup`, and `Wininet.lib` from `TuniacApp` if nothing else needs it.
- [ ] Decide `static=true` (original file, BASS must decode it) vs. letting Jellyfin
      transcode (`/Audio/<id>/universal`) for formats BASS can't play.
- [ ] Not logged in / token expired → fail with a logged reason, skip to next track.

Known limitation to accept for now: the `\r\n` header trick only works with
`bass_Plugin`. Returning URL and headers separately means changing `IAudioSource`;
postpone until a second decoder needs it.

## Phase 4 — Getting songs into Tuniac (long term)

Prerequisite for every option below: a host API that adds a library entry **with
metadata** (title, artist, album, track no., duration, year, genre) for a URL.
`AddStreamToLibrary` only sets the URL and puts it in the artist field. Needs a new
helper-interface method, e.g. `AddEntry(LibraryEntry*)`, or a service-flavoured variant.

Options for how songs get there:

| | How | Pros | Cons |
|---|---|---|---|
| **A. Search & add** | Plugin dialog: search box → `GET /Items?searchTerm=…&IncludeItemTypes=Audio&Recursive=true` → pick tracks/albums → add to library or current playlist | Small, user in control, no sync logic | Library only has what you added; no browsing |
| **B. Full sync** | Pull all audio items into the library; periodic/incremental refresh (`MinDateLastSaved`) to pick up new and removed songs | Feels like local music; library search/sort just work | Large libraries, sync/deletion bookkeeping, startup cost |
| **C. Live source view** | A new `ISourceView` / playlist type that browses the server directly, nothing stored | Always current, no sync | Biggest UI change; Tuniac's views assume the library |

Suggestion: **start with A**. It needs the same "add entry with metadata" plumbing that B
would, so B can be added later as "add everything + refresh" without throwing work away.
Revisit C only if A/B feel wrong in use.

Decisions to make before Phase 4:

- [x] URL format: `jellyfin://<serverid>/<itemid>` (decided — no single-server assumption).
- [ ] What happens to library entries when logged out or the server is gone — keep and
      fail at play time (simplest), or hide.
- [ ] Album art: fetch `/Items/<id>/Images/Primary` on play, or cache.
- [ ] Report playback back to the server (`/Sessions/Playing`, `…/Stopped`) so play
      counts / "continue listening" work in Jellyfin. Nice to have.

## Housekeeping (whenever)

- [ ] Remove `Jellyfin_Plugin.aps` from git and add `*.aps` to `.gitignore`.
- [ ] Remove the empty `.vscode/launch.json`.
- [ ] Fix byte-vs-char `Size` and `PluginGetValue` returning true on missing values (§5.7)
      — matters once Phase 1 relies on "is there a token?".
