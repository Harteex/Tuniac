# Jellyfin / service plugins — plan

Working plan for the `jellyfin` branch. Tick boxes as things land. Background and
line-level detail: [service-plugins.md](service-plugins.md) (referenced below as §n).

Status: **plugin type loads, nothing works end to end.** Login code exists but has no
caller and a body-encoding bug; no audio plays; nothing reaches the library.

---

## Phase 0 — Clear the ground (small, do first)

Cheap fixes that would otherwise bite during the later phases.

- [ ] `PrefPage m_Pages[11]` → `[12]` in `Preferences.h` (§5.2). Currently overwrites
      memory and hides the EQ page.
- [ ] Version check: export/look up `GetTuniacServicePluginVersion` and compare against
      `ITUNIACSERVICEPLUGIN_VERSION` (§5.3).
- [ ] `ServicesProc`: use `SERVICEPLUGINFLAGS_*`, not `PLUGINFLAGS_*` (§5.4).
- [ ] Decide the registry home now, before tokens get stored: move service plugins to
      `HKCU\Software\Tuniac\services\…` (§5.5). Moving later means migrating saved logins.
- [ ] Store the plugin's `HINSTANCE` from `DllMain` instead of
      `GetModuleHandle(L"Jellyfin_Plugin.dll")` (§5.7).

## Phase 1 — Authentication

Goal: given server URL + username + password, end up with a persisted token and user
id, and be able to tell on startup whether we are still logged in.

- [ ] **Fix the POST body** (§5.1): `CHttpService::Post` takes `const char*` + byte
      length; `Login()` sends RapidJSON's UTF-8 buffer directly, no UTF-16 round trip.
- [ ] **Send the client authorization header on every request, including login.**
      Jellyfin wants `Authorization: MediaBrowser Client="Tuniac", Device="…",
      DeviceId="…", Version="…"` (plus `, Token="…"` once logged in). Current `Login()`
      sends only `Content-Type`; newer servers reject that. Build it in one helper.
- [ ] **Stable DeviceId**: generate a GUID once and store it. The prototype used the
      computer name; Jellyfin ties sessions to DeviceId, so it must be unique and stable.
- [ ] **Parse and keep more than the token**: `AccessToken`, `User.Id`, `User.Name`,
      `ServerId`. User id is needed for most item queries later.
- [ ] **Persist** server URL, user id, user name, token via `PreferencesSet`,
      **keyed by `ServerId`** (e.g. `…\services\Jellyfin\Servers\<serverid>\…`), since
      links carry the server id (see URL format below). One login at a time in the UI is
      fine to start with, but the storage must not assume a single server.
      Never store the password.
- [ ] **Validate on startup**: `GET /Users/Me` (or `/System/Info`) with the token; on
      401 mark as logged out rather than failing silently at playback time.
- [ ] **Logout**: `POST /Sessions/Logout`, then clear token/user id locally (clear
      locally even if the server call fails).
- [ ] **Error reporting**: return something better than `bool` from `Login()` —
      at least distinguish "can't reach server" / "bad credentials" / "unexpected reply",
      and log via `LogMessage`.
- [ ] **TLS**: drop the unconditional ignore-cert flags (§5.6); add an opt-in
      "allow self-signed certificate" setting if needed for home servers.

Open: does login run on the UI thread with a wait cursor (simplest; WinINet calls are
blocking) or on a worker thread? Suggest UI thread + timeout for now.

## Phase 2 — Plugin configuration UI

Goal: the Configure dialog is the login/logout screen.

- [ ] Replace the PopupNotify-cloned dialog (`IDD_NOTIFYPREFWINDOW`) with a proper one:
      Server URL, Username, Password, **Log in** / **Log out** buttons, status line
      ("Logged in as X on Y" / "Not logged in" / last error).
- [ ] Logged-in state disables the credential fields and shows Log out; logged-out the
      reverse. Pre-fill server URL and user name from settings.
- [ ] Normalise the server URL (trim trailing `/`, require `http://` or `https://`).
- [ ] Remove the commented-out PopupNotify body from `DlgProc`, rename leftovers
      (`pPopupNotify`, dialog ID).
- [ ] Optional: "Test connection" button hitting `/System/Info/Public` (no auth needed).

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
      `bool TranslateSource(LPTSTR szDest, size_t cchDest, LPTSTR szSource)`. Bump
      `ITUNIACSERVICEPLUGIN_VERSION`.
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
