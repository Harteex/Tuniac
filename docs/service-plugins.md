# Service plugins and the Jellyfin plugin (`jellyfin` branch)

Analysis of the work on the `jellyfin` branch, branched from `master` at `dac98a6`.
Read [architecture.md](architecture.md) first for how the rest of the player fits
together. Line references are as of `64b8b8c`.

---

## 1. What the branch is trying to do

Let Tuniac play music from a Jellyfin server — and do it as a **new, general plugin
family** ("service plugins") rather than as Jellyfin-specific code inside the app, so
other streaming backends can be added the same way later.

That intent is visible in the code, in `CCoreAudio::SetSource`
(`Tuniac1/TuniacApp/CoreAudio.cpp:253`):

```cpp
// TODO We should check an array of services here
// Also move these services to a new type of plugin
```

## 2. How it got here

```
* 64b8b8c WIP                    <- HTTP + JSON layer, Jellyfin login, rapidjson vendored
* f244eff Implement the ServicePluginManager …
*   0c7f763 Merge
|\
| * 27119ba Improve buffer handling and error checking      (duplicate of f219322)
| * 42bce1e Implement a first version of playing music …    (duplicate of b99c82a)
* a70e41b WIP Implement preferences dialog for new service plugin
* 7a5c916 WIP Make Jellyfin a new type of plugin
* f219322 Improve buffer handling and error checking
* b99c82a Implement a first version of playing music from a Jellyfin server …
```

Two phases:

**Phase 1 — the working prototype (`b99c82a`, `f219322`).** A `CJellyfinService` class
compiled straight into `TuniacApp`, hooked into `CCoreAudio::SetSource`. It rewrote a
`jellyfin://<itemid>` URL into a real Jellyfin stream URL, and this *did* play audio.

**Phase 2 — generalising it into a plugin type (`7a5c916` onward).** New
`ITuniacServicePlugin` interface, a `Jellyfin_Plugin` DLL project cloned from
`PopupNotify`, a "Services" preferences page, and `CServicePluginManager` to discover and
load DLLs from `services\`. Then a reusable WinINet `CHttpService` plus RapidJSON, and a
real `Login()` that authenticates against Jellyfin and extracts an access token.

The merge at `0c7f763` folded in a remote copy of the same first two commits (they were
rebased locally and then merged back), which is why the log shows each of them twice. No
content difference — just noise in the history.

## 3. What exists now, file by file

### 3.1 The interface — `TuniacApp/ITuniacServicePlugin.h`

Modelled directly on `ITuniacPlugin.h`, with the process-wide bits removed (commented out
rather than deleted):

```cpp
#define ITUNIACSERVICEPLUGIN_VERSION  MAKELONG(0,4)

class ITuniacServicePluginHelper {          // implemented by the host
    virtual void LogMessage(LPTSTR module, LPTSTR msg) = 0;
    virtual bool PreferencesGet(...) = 0;
    virtual bool PreferencesSet(...) = 0;
};

class ITuniacServicePlugin {
    virtual void          Destroy(void) = 0;
    virtual LPTSTR        GetPluginName(void) = 0;
    virtual unsigned long GetFlags(void) = 0;             // SERVICEPLUGINFLAGS_ABOUT/_CONFIG
    virtual bool          SetHelper(ITuniacServicePluginHelper*) = 0;
    virtual bool          About(HWND) = 0;
    virtual bool          Configure(HWND) = 0;
};

extern "C" __declspec(dllexport) ITuniacServicePlugin* CreateTuniacServicePlugin(void);
extern "C" __declspec(dllexport) unsigned long GetTuniacServicePluginVersion(void);
```

The commented-out `CreateThread` / `GetPluginWindow` are inherited from `ITuniacPlugin`,
where notification plugins each own a thread. A service plugin does not need one yet.

**This interface has no service-specific methods at all.** It can identify itself and show
dialogs; it cannot yet be asked "can you handle this URL", "give me a playable stream
URL", "list the albums on the server". That is the central gap — see §6.

### 3.2 The loader — `TuniacApp/ServicePluginManager.{h,cpp}`

A stripped-down copy of `CPluginManager` (the commit message says so), same lifecycle:

- `Initialize()` enumerates `<exedir>\services\*.dll`, version-checks, calls
  `CreateTuniacServicePlugin`, reads the enable flag from
  `HKCU\Software\Tuniac\plugins` keyed by DLL filename, and keeps disabled plugins in the
  array as name-only entries so preferences can still list them.
- `EnablePlugin(i, bool)` loads/unloads the DLL live.
- `Shutdown()` persists the enable flags and unloads everything.
- Implements `ITuniacServicePluginHelper`: `LogMessage` to `CLogWindow`, and
  `PreferencesGet`/`Set` prefixing the subkey with `plugins\`.

Threading, `PostMessage` broadcast, `GetVariable`, `GetTrackInfo` and `Navigate` are all
commented out relative to `CPluginManager`.

Wired into the app in `TuniacApp.cpp` — `m_ServicePluginManager.Initialize()` right after
`m_PluginManager.Initialize()`, and `Shutdown()` before the plugin manager's — with the
member declared in `TuniacApp.h`.

### 3.3 Preferences — "Services" page

`CPreferences::ServicesProc` (`Preferences.cpp:930+`) is a near-copy of `PluginsProc`: a
checkbox list view of service plugins, with About / Configure / Enable All / Disable All.
The dialog `IDD_PREFERENCES_SERVICES` and the `IDC_SERVICEPLUGIN_*` control IDs were added
to `Tuniac.rc` / `resource.h`. The page is registered as `m_Pages[9]` under the Plugins
parent, pushing Visuals to 10 and EQ to 11.

### 3.4 The plugin — `Tuniac1/Jellyfin_Plugin/`

Cloned from `PopupNotify` (the dialog is still called `IDD_NOTIFYPREFWINDOW` and local
variables are still named `pPopupNotify`). Builds `Jellyfin_Plugin.dll` into
`$(Platform)\$(Configuration)\services\`, links `shlwapi.lib` and `wininet.lib`, and adds
`../TuniacApp` to the include path so it can see `ITuniacServicePlugin.h`.

- `JellyfinService.cpp` — the `ITuniacServicePlugin` implementation. `GetPluginName()`
  returns `"Jellyfin"`, `GetFlags()` returns `ABOUT | CONFIG`, `About()` shows a message
  box. `Configure()` opens a placeholder dialog: one unused edit box and an OK button, and
  a `DlgProc` that is almost entirely the commented-out PopupNotify body.
- `Login(serverUrl, username, password, tokenOut, tokenSize)` — the one piece of real
  Jellyfin logic. Builds `{"Username":…,"Pw":…}` with RapidJSON, POSTs it to
  `<server>/Users/AuthenticateByName`, parses the reply and copies `AccessToken` out.
  **Nothing calls it**, and it does not persist the token anywhere.
- `HttpService.{h,cpp}` — a small reusable WinINet client: `Get`/`Post`, a hand-written
  `ParseUrl` (scheme, host, port, path), an `HttpResponse` struct with status code,
  content buffer, length and error code, and growing-buffer reads. Deliberately generic,
  so it could move into the app or a shared header later.
- `rapidjson/` — vendored header-only JSON library (~19k lines, the bulk of the branch's
  diff). This is the only vendored third-party code in the repo.

### 3.5 The prototype still in the app — `TuniacApp/JellyfinService.{h,cpp}`

The phase-1 class, still compiled into the EXE and still constructed by
`CCoreAudio::Startup` (`CoreAudio.cpp:183`), but **no longer used**: the call site in
`SetSource` is commented out (`CoreAudio.cpp:253-263`).

What it does when enabled:

- `CanHandle()` matches the `jellyfin://` prefix.
- `TranslateSource()` turns `jellyfin://<itemid>` into

  ```
  <host>/Audio/<itemid>/stream?static=true\r\nX-Emby-Authorization: MediaBrowser Client="Tuniac", Device=…, DeviceId=…, Version=…, Token=…\r\n
  ```

  The `\r\n` plus header is not a malformed URL — it is BASS's convention for
  `BASS_StreamCreateURL`, which accepts extra request headers appended after the URL. So
  the rewritten string flows through `SetSource` into `bass_Plugin`, whose `CanHandle`
  sees a URL and opens it. That is how phase 1 played audio with no new decoder code.
- Host and token are read at construction from `HKCU\Software\Tuniac\plugins\Jellyfin`
  (`Host`, `Token`) — values that had to be written into the registry by hand.
- `Login()` is an empty stub; `DoHttpRequest()` (raw Winsock) and `DoHttpRequest2()`
  (WinINet) are scratch experiments against hardcoded hosts, superseded by the plugin's
  `CHttpService`.

`TuniacApp.vcxproj` gained `Wininet.lib` and both new source pairs.

## 4. What works today, and what does not

Works:

- `Jellyfin_Plugin.dll` builds into `services\`, is discovered, version-checked, loaded and
  listed on the new Services preferences page; the enable checkbox toggles it live and the
  state survives a restart; About and Configure open.
- `CServicePluginManager` gives the plugin logging and registry-backed settings.
- `Login()` implements the Jellyfin authentication exchange (subject to the encoding bug
  in §5.1).

Does not work:

- **No Jellyfin audio plays on this branch.** The only code that ever produced playback is
  the commented-out block in `SetSource`.
- The plugin cannot influence playback at all — `ITuniacServicePlugin` has no method for
  it, and `CCoreAudio` never asks `CServicePluginManager` anything.
- `Configure()` cannot capture a server URL, username or password, so `Login()` has no
  inputs and no caller.
- Nothing persists a token; nothing browses the server's library; no Jellyfin item ever
  reaches the media library or a playlist.

## 5. Known issues

### 5.1 Login POSTs UTF-16 as if it were UTF-8 — `Jellyfin_Plugin/HttpService.cpp:216`

```cpp
BOOL bSendResult = HttpSendRequest(hRequest, NULL, 0, szData,
        (szData != NULL) ? (DWORD)(_tcslen(szData) * sizeof(TCHAR)) : 0);
```

`Login()` builds the JSON as UTF-8, converts it to `TCHAR` (UTF-16), and hands it to
`Post()`, which sends those raw UTF-16 bytes as the request body with
`Content-Type: application/json` (`JellyfinService.cpp:275`). WinINet does not transcode.
Jellyfin will see a byte sequence with embedded NULs and reject it. Fix: keep the body as
UTF-8 `char*` end to end — change `Post`/`PerformRequest` to take `const char* pBody,
DWORD cbBody` and drop the UTF-16 round trip in `Login()`.

### 5.2 `CPreferences::m_Pages` overflows — `Preferences.h:203`, `Preferences.cpp:2073`

`PrefPage m_Pages[11];` still declares eleven slots, but the constructor now writes
`m_Pages[0]` through `m_Pages[11]` — twelve pages. Two consequences:

- Writing `m_Pages[11]` (the EQ page) runs past the array, over whatever follows it in
  `CPreferences` (`m_StartPage` and onward).
- Every loop sizes itself with `sizeof(m_Pages) / sizeof(PrefPage)` == 11, so the EQ page
  is never inserted into the tree — it silently disappears from Preferences.

Fix: `PrefPage m_Pages[12];`.

### 5.3 Service plugins are version-checked against the wrong symbol — `ServicePluginManager.cpp:46`

```cpp
GetTuniacPluginVersionFunc pGTPVF =
    (GetTuniacPluginVersionFunc)GetProcAddress(hDLL, "GetTuniacPluginVersion");
if (pGTPVF() != ITUNIACPLUGIN_VERSION)
```

It looks up `GetTuniacPluginVersion` and compares against `ITUNIACPLUGIN_VERSION` — the
*general* plugin symbol and constant. `ITuniacServicePlugin.h` declares
`GetTuniacServicePluginVersion`, but `Jellyfin_Plugin` actually exports
`GetTuniacPluginVersion` returning `ITUNIACSERVICEPLUGIN_VERSION`, so it happens to line
up today only because both constants are `MAKELONG(0,4)`. The moment either version moves
independently this breaks, and today an ordinary `plugins\` DLL dropped into `services\`
would pass the check and then fail at `CreateTuniacServicePlugin`. Fix: export and look up
`GetTuniacServicePluginVersion`, compare against `ITUNIACSERVICEPLUGIN_VERSION`.

### 5.4 Flag constants crossed in the Services page — `Preferences.cpp:996, 1005, 1078`

`ServicesProc` tests `PLUGINFLAGS_ABOUT` / `PLUGINFLAGS_CONFIG` where it means
`SERVICEPLUGINFLAGS_*` (line 1058 gets it right). Harmless while both pairs are `0x1`/`0x2`,
but it hides a real dependency between two unrelated headers.

### 5.5 Shared registry namespace — `ServicePluginManager.cpp:83` (the `FIXME??`)

Service plugins store their enabled flag under `HKCU\Software\Tuniac\plugins` keyed by DLL
name, and their settings under `plugins\<name>` — the same namespace as ordinary plugins.
A `services\Foo.dll` and a `plugins\Foo.dll` would collide. A `services` subkey would keep
them apart.

### 5.6 TLS certificate validation is disabled — `Jellyfin_Plugin/HttpService.cpp:182`

```cpp
dwFlags |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
```

Set unconditionally for every HTTPS request, with the comment "may need to adjust for
production". Since these requests carry a username and password, this should at minimum be
an opt-in per-server setting rather than the default.

### 5.7 Smaller things

- `CJellyfinService::Configure` (plugin) resolves its resources with
  `GetModuleHandle(L"Jellyfin_Plugin.dll")` — a rename of the DLL silently breaks the
  dialog. Storing the `HINSTANCE` from `DllMain` is the usual fix.
- `DlgProcStub` calls through a pointer read from `GWLP_USERDATA`, which is still 0 for
  messages that arrive before `WM_INITDIALOG`. Inherited from `PopupNotify` and harmless
  only because `DlgProc` never touches `this`.
- App-side `CJellyfinService` ctor (`TuniacApp/JellyfinService.cpp:50-52`) passes `Size` as
  256/128 to `PluginGetValue`, which forwards to `RegQueryValueEx`, where the unit is
  **bytes**, not characters — so it reads at most half of each buffer. Not an overflow, but
  not what the numbers suggest.
- `CPreferences::PluginGetValue` returns `true` whenever the key opens, ignoring
  `RegQueryValueEx`'s result — a missing value reads as success. Pre-existing on `master`,
  but the Jellyfin code relies on it.
- `Jellyfin_Plugin.aps` (the resource editor's cache) is committed; it is a build artifact
  and could be added to `.gitignore`.
- `.vscode/launch.json` was committed with an empty configuration list.

## 6. The design decision still open

Everything above is mechanics. The substantive question the branch has not answered is
**what a service plugin is actually for**, which decides what goes in
`ITuniacServicePlugin`. Two roles are already visible in the prototype:

1. **URL resolution.** "Given `jellyfin://<id>`, produce something the audio pipeline can
   open." The prototype did exactly this. Its natural home is a method like
   `bool CanHandle(LPTSTR szSource)` plus `bool TranslateSource(LPTSTR dest, size_t, LPTSTR src)`
   on the interface, with `CCoreAudio::SetSource` walking
   `tuniacApp.m_ServicePluginManager` before it walks `m_AudioSources` — replacing the
   `TODO` at `CoreAudio.cpp:253`. That alone restores playback, and it is the smallest
   next step.

   Two wrinkles when doing it: the translated string must stay alive for the whole of
   `SetSource` (the commented-out code assigns a pointer to a local buffer to `szSource`),
   and the BASS "URL + `\r\n` + headers" trick is a BASS-specific convention — anything
   that is not the BASS decoder will not understand the result. Returning URL and headers
   separately would be more honest, though it means touching `IAudioSource`.

2. **Content browsing.** For the library and the source tree to show a Jellyfin server, a
   service needs to enumerate artists/albums/tracks and hand back entries — a bigger
   design touching `IPlaylist`/`ISourceView` and probably a new playlist type
   (`PLAYLIST_TYPE_*`), plus a decision about whether remote tracks are cached in the
   media library (as `ENTRY_KIND_URL` entries, which `AddStreamToLibrary` already
   supports) or fetched live.

A pragmatic order: fix §5.2 and §5.1, add the resolution methods and re-enable the
`SetSource` hook so audio plays again through the plugin, build the Configure dialog so
`Login()` has inputs and store the token via `PreferencesSet`, and only then take on
browsing.
