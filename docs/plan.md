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

**URL format (decided):** `jellyfin://<serverid>/<itemid>`. Multiple servers are allowed;
the plugin looks up URL and token by server id, so links to any server we've logged in to
keep playing, not only the active one.

**Correction to the earlier note:** `PathIsURL` returns TRUE for *any* `scheme://…` string,
including `jellyfin://` (checked). So the core already treats these links as stream URLs:
`AddItem` stores them as `ENTRY_KIND_URL` entries (File → Import Stream… works), tag
reading skips them, "delete file from disk" skips them. No host-side check was needed for
adding. Side effects to remember for Phase 4: entries get `LENGTH_STREAM`, and the
"Automatically skip streams" option skips them in shuffle.

- [x] `ITuniacServicePlugin` gained `CanHandle(LPCTSTR)` and
      `TranslateSource(LPTSTR, size_t, LPCTSTR)`; `SERVICE_SOURCE_MAX` (2048) is the buffer
      size. No version bump (unreleased interface, plugin built with the app).
- [x] `CServicePluginManager::CanHandle` / `TranslateSource` walk the enabled plugins.
- [x] `CCoreAudio::SetSource` resolves service links before the decoder loop. If a service
      owns the link but can't resolve it, playback fails straight away (the app logs it,
      marks the entry unavailable and moves on).
- [x] **Lifetime fix (the prototype's bug, and a second one):** decoders keep the string
      they're given — BASS passes it to `UpdateMetaData` and registers it as `MetaSync` user
      data — so a stack buffer would dangle. The resolved string is `new[]`'d and owned by
      the `CAudioStream` (`SetResolvedSource`), freed in its destructor after the decoder is
      destroyed. The stream itself still gets the original link, so the play count
      (`NOTIFY_COREAUDIO_PLAYEDPOINTREACHED`) reaches the library entry, and the "Set source"
      log line doesn't show the token.
- [x] Jellyfin plugin: `jellyfin://<server>/<item>` →
      `<url>/Audio/<item>/stream?static=true` + `\r\n` + the `Authorization` header with
      the token. Clear log messages for malformed links and "not logged in to that server".
- [x] `bass_Plugin`: URL buffer 512 → 4096 bytes (a resolved link is ~260 chars), and fail
      cleanly instead of passing a truncated URL.
- [x] Deleted the prototype `TuniacApp/JellyfinService.{h,cpp}`, its use in `CCoreAudio`,
      and `Wininet.lib` from TuniacApp's Debug|x64 link line (nothing else used it).
- [x] Decided `static=true` (original file). Fine while BASS can decode what the server
      stores; `/Audio/<id>/universal` (server transcodes) is the fallback if that bites.

Tested outside Tuniac with the real code: the plugin's `TranslateSource` plus the real
`bass_Plugin.dll` through `IAudioSourceSupplier`, as `SetSource` calls it, against
`demo.jellyfin.org/stable`. An MP3 and a FLAC track both opened (correct length, rate,
channels) and decoded 5 s of real audio. Not-logged-in and malformed links fail with the
expected messages.

Known limitations, deliberately left for later:

- **Metadata from the decoder doesn't reach the entry.** BASS reports length, bitrate,
  file type under the resolved URL, which matches no library entry. Phase 4 will set this
  from Jellyfin's own data instead; mapping it back in `CCoreAudio::UpdateMetaData` is the
  alternative if needed.
- **No seeking.** `bass_Plugin` opens every URL with `BASS_STREAM_BLOCK` (radio-style).
  Jellyfin tracks are finite files and could be opened seekable; needs a way for the
  decoder to tell the two apart.
- **"Allow untrusted HTTPS certificates" doesn't apply to playback** — BASS does its own
  HTTPS. Self-signed servers can log in but not stream.
- ~~Expired token at play time shows up only as a generic "failed to playback".~~ Fixed:
  when no decoder can open a resolved link, `CCoreAudio` calls the new
  `ITuniacServicePlugin::SourceFailed`, and the Jellyfin plugin asks the server about the
  item and logs the real cause (login expired / no such item / it's an album, not a track /
  track exists but couldn't be streamed or decoded, with its format). `bass_Plugin` also
  logs the BASS error code for URLs it can't open (not the URL, which carries the token).
  Found when a test used an album id from Jellyfin's web UI, which uses the same
  `details?id=` URL for albums and tracks.
- **Service plugin disabled:** the raw `jellyfin://` link goes to BASS, which fails it.
- The `\r\n` header trick is BASS-specific; returning URL and headers separately means
  changing `IAudioSource`. Postponed until a second decoder needs it.
## Phase 4 — Getting songs into Tuniac

Chose **A. Search & add** first (B, full sync, can reuse the same plumbing later; C, a
live source view, only if A/B feel wrong). Decided: opened from the File menu, finds
tracks and albums, adds to the library only.

### 4a — Search & add (done)

- [x] Interface: `ServiceTrack` (a POD with the library fields a service can fill in),
      `ITuniacServicePluginHelper::AddToLibrary(ServiceTrack*, count)`,
      `SERVICEPLUGINFLAGS_IMPORT` and `ITuniacServicePlugin::Import(HWND)`. Still no version
      bump.
- [x] Host: `CMediaLibrary::AddServiceTracks` stores entries as given (`ENTRY_KIND_URL`,
      real length/bitrate/etc., no tag reading), skips links already in the library, does
      *not* put them in the "Streams" playlist, rebuilds the library view once per batch.
- [x] Host: File menu gets "Import from <name>..." for each enabled service plugin with
      the import flag, rebuilt on `WM_INITMENUPOPUP` so enabling/disabling is picked up
      live. Command IDs `SERVICEMENU_BASE` (49900) + plugin index.
- [x] Host: "Automatically skip streams" (on by default) used `PathIsURL`, so it would
      have skipped every Jellyfin track in shuffle. Now only URL entries with no fixed
      length (`LENGTH_STREAM`, i.e. radio) count as streams (`IsSkippableStream` in
      `BasePlaylist.cpp`).
- [x] Jellyfin: `IDD_JELLYFIN_IMPORT` dialog — search box, results list (albums first, then
      tracks; type, name, artist, album, length), multi-select, Add to library (or
      double-click), status line ("Added N tracks (M were already there)").
- [x] Search = `/Items?searchTerm=…&IncludeItemTypes=MusicAlbum,Audio` (max 200), plus
      the albums of artists whose name matches (`/Artists?searchTerm=…` →
      `/Items?ArtistIds=…`), because `searchTerm` only matches item *names* — searching
      an artist otherwise finds nothing.
- [x] Adding an album fetches its tracks (`ParentId=…`, sorted by disc/track). Track
      metadata from Jellyfin: title, artists, album, album artist, genres, year, track/disc
      number, length (`RunTimeTicks`), container, bitrate, sample rate, channels, bit depth.

Verified by driving the real DLL's dialog against `demo.jellyfin.org/stable` with a fake
host: artist search → 2 albums → added → 3 tracks with full metadata. **Not yet tried
inside Tuniac**, i.e. the File menu item and `AddServiceTracks` themselves.

### Next / open

- [x] Try in Tuniac: import, play, shuffle, restart (library persistence), import again
      (duplicates skipped).
- [x] Restart re-selected the first track instead of the last-played Jellyfin one: service
      plugins were loaded after the playlists were restored. Now loaded first (`fdb1a25`).
- [ ] Track length from the decoder still doesn't reach the entry (Phase 3 note), but it's
      no longer needed: the length now comes from Jellyfin.
- [ ] Seeking (BASS opens URLs with `BASS_STREAM_BLOCK`) — more noticeable now that these
      look like normal tracks.
- [ ] Search is on the UI thread; slow servers freeze the dialog until the timeout.
- [ ] Composer isn't filled (Jellyfin has it under `People`, not a plain field).
- [ ] Entries when logged out or the server is gone: currently kept, fail at play time
      with a clear log message. Probably fine.
- [x] Notice when the login has expired or was revoked ("Log in again now?" → Configure):
      shown when playback or the import search is rejected, then the token is forgotten.
      No check at startup, to keep startup fast. Note: Jellyfin serves
      `/Audio/<id>/stream` without checking the token (verified on the demo server), so
      playback with a revoked token usually still works and the notice mostly comes from
      import.
- [ ] Album art: fetch `/Items/<id>/Images/Primary` on play, or cache.
- [ ] Report playback to the server (`/Sessions/Playing`, `…/Stopped`). Nice to have.
- [ ] Later: B (sync everything + refresh), reusing `AddToLibrary`.

## Standalone — Don't open the restored song at startup

At startup the last active entry is restored with `PlayEntry(…, bStart=false)`
(`PlaylistManager.cpp`, the restore blocks around the `PLDH.ActiveIndex` checks). That opens
the file or stream just to select it: a network request for Jellyfin entries and radio
streams, a startup delay when the server is down, and the selection falls back to the first
track if opening fails. It also adds the song to history and updates the queue.

Play already handles a song that isn't loaded: `ID_PLAYBACK_PLAY` falls back to
`PlayEntry(active entry, true, …)` when `CCoreAudio::Play()` fails. So startup only needs to
select the entry and load its art.

- [ ] Small helper that sets the active entry and loads album art without opening it; use
      it in the restore blocks in `PlaylistManager.cpp` instead of `PlayEntry`.
- [ ] Play controls show nothing for length/position until playing — show the library's
      `FIELD_PLAYBACKTIME` while nothing is loaded.
- [ ] Accept: seeking while stopped does nothing until Play; missing files are noticed at
      Play rather than at startup; startup no longer adds a history entry.
- [ ] Test with a local file, a radio stream and a Jellyfin entry (server up and down).

Estimate: 30–60 lines.

## Housekeeping (whenever)

- [ ] Remove `Jellyfin_Plugin.aps` from git and add `*.aps` to `.gitignore`.
- [ ] Remove the empty `.vscode/launch.json`.
- [ ] Fix byte-vs-char `Size` and `PluginGetValue` returning true on missing values (§5.7)
      — matters once Phase 1 relies on "is there a token?".
