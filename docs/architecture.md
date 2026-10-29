# Tuniac architecture

How the player is put together, written from a read of the `Tuniac1/` tree. Paths are
relative to the repository root. This describes the code as it exists on the `jellyfin`
branch; anything added by that branch is marked **[branch]** and covered properly in
[service-plugins.md](service-plugins.md).

---

## 1. Shape of the repository

Three top-level trees, only one of which is live:

| Tree | Status |
|---|---|
| `Tuniac1/` | **The product.** Everything in `TuniacApp.sln` builds from here. |
| `Tuniac2/` | An abandoned rewrite (`Tuniac2.sln`, `Globals`, `GenericInfoReader`, `History`). Not referenced by Tuniac 1 and not maintained. |
| `Old/` | Per-format decoder plugins from before BASS took over — mp3, ogg, FLAC, ape, wma, wv, tta, mpc, m4a, alac, wav, CDDA, plus a couple of info/notify plugins. Not in the solution; useful only as examples of the decoder interface. |

`Tuniac1/TuniacApp/` is the application itself; every sibling directory in `Tuniac1/` is a
plugin DLL or tooling.

Supporting material lives in `Tuniac1/Housekeeping/` — `How_To_Build.txt` (the real build
instructions), `TODO.txt`, `Ideas.txt`, `Change Log.txt`, `Tokens.txt` and
`Window Title Codes.txt` (the format-string language used by the title bar and the
notification plugins), plus the `build_*.cmd` scripts for the external libraries.
`Tuniac1/Guide/` is the end-user HTML manual, `Tuniac1/Setup-Scripts/` the Inno Setup
installer scripts.

## 2. Build and toolchain

- Solution: `Tuniac1/TuniacApp.sln`, Visual Studio 2022 (format 12, VS version 17).
- Platforms `Win32` / `x64`; configurations `Debug`, `Release`, `Release_xp`. `Release_xp`
  pins `v141_xp` so a build still runs on XP; the rest use `v143`.
- Character set: Unicode for every project. All string handling is `TCHAR`-based and in
  practice always UTF-16.
- External dependencies are expected as **siblings of the repository root**, built
  beforehand with the scripts in `Housekeeping/`: `../zlib`, `../taglib`, `../libpng`,
  `../libjpeg-turbo`. BASS ships prebuilt inside `Tuniac1/bass_Plugin/basslib/`. XAudio2
  arrives through the `Microsoft.XAudio2.Redist` NuGet package
  (`Tuniac1/TuniacApp/packages.config`, restored to `Tuniac1/packages/`).
- Build output is arranged so the runtime loaders can find things:
  `Tuniac1/$(Platform)/$(Configuration)/` holds `Tuniac.exe` and the decoder DLLs,
  `.../plugins/` holds the ordinary plugins, and `.../services/` holds service plugins
  **[branch]**. Each plugin project sets `OutDir` accordingly.
- There is no CI, no CMake, no test project. Verification is "build the solution and run
  it".

## 3. Startup and the global object

`TuniacApp/Tuniac.cpp` is the whole entry point:

```cpp
CTuniacApp    tuniacApp;          // the global everything hangs off
CRandomSFMT   g_Rand(time(NULL));

int APIENTRY WinMain(...)
{
    if(!tuniacApp.Initialize(hInstance, GetCommandLine())) return 0;
    tuniacApp.Run();
    tuniacApp.Shutdown();
}
```

`CTuniacApp` (`TuniacApp.h`, and a ~100 KB `TuniacApp.cpp`) is a deliberate god object.
Its public members *are* the subsystems, and every other file reaches them through the
`tuniacApp` global rather than through injected pointers:

```
CMediaLibrary          m_MediaLibrary
CPlaylistManager       m_PlaylistManager
CPreferences           m_Preferences
CPlayControls          m_PlayControls
CAlbumArt              m_AlbumArtPanel
CSourceSelectorWindow* m_SourceSelectorWindow
CVisualWindow*         m_VisualWindow
CLogWindow*            m_LogWindow
CPluginManager         m_PluginManager
CServicePluginManager  m_ServicePluginManager     [branch]
CTaskbar               m_Taskbar
CHistory               m_History
CPlayQueue             m_Queue
CTuniacSkin            m_Skin
```

`Initialize()` runs roughly: single-instance mutex → load preferences → media library →
playlists → register the window class and create the main window → `m_PluginManager
.Initialize()` → `m_ServicePluginManager.Initialize()` **[branch]** → tray menu →
taskbar → process the command line → show the window → hotkeys. `Shutdown()` unwinds in
the mirror order, saving the playlist DB and media library first, then shutting the
plugin managers down (services before plugins) and writing preferences back.

Audio is *not* started here: `CCoreAudio` is a singleton (`CSingleton<CCoreAudio>`,
`Singleton.h`) brought up on first use via `CCoreAudio::Instance()`.

## 4. Audio pipeline

```
IPlaylistEntry (FIELD_URL)
      │  CTuniacApp::PlayEntry()
      ▼
CCoreAudio::SetSource(szSource, replaygain…)          CoreAudio.cpp
      │   asks each loaded IAudioSourceSupplier: CanHandle(szSource)?
      │   the winner returns an IAudioSource
      ▼
CAudioStream  (one per playing/fading track)          AudioStream.cpp
      │   service thread pulls float buffers from IAudioSource
      │   applies EQ (EQSTATE, 3-band), replaygain, fade in/out volume
      ▼
CAudioOutput / IXAudio2SourceVoice ──► XAudio2 mastering voice ──► device
```

Details worth knowing:

- **Decoder discovery.** `CCoreAudio::Startup()` enumerates `*.dll` **next to the EXE**
  (not in `plugins\`), `LoadLibrary`s each, and looks for the exported
  `CreateDecoderPlugin`. Anything that answers becomes an `IAudioSourceSupplier` in
  `m_AudioSources`. Note the mismatch between the exported name `CreateDecoderPlugin` and
  the header's declared `CreateAudioSourceSupplier` in `IAudioSource.h` — the loader uses
  the former.
- **Format selection is by probe, not extension.** `SetSource` walks `m_AudioSources` in
  load order and takes the first whose `CanHandle()` returns true. `bass_Plugin` decides
  by actually trying `BASS_StreamCreateFile` / `BASS_StreamCreateURL`, so it also answers
  for URLs (`PathIsURL`) and for the pseudo-scheme `AUDIOCD:<drive>:<track>`.
- **Crossfading** is why `m_Streams` is an array: a new `CAudioStream` starts and fades in
  while the old one fades out, and `CheckOldStreams()` reaps the finished ones.
  `NOTIFY_COREAUDIO_MIXPOINTREACHED` and friends tell the app when to advance.
- **File I/O abstraction.** `IAudioFileIO` lets a decoder read through the host:
  `TuniacStandardFileIO`, `TuniacMemoryFileIO`, `TuniacHTTPFileIO`. A supplier advertises
  what it wants via `FLAGS_PROVIDESTANDARDFILEIO` / `…MEMORYFILEIO` / `…HTTPFILEIO`.
  BASS does its own I/O and does not use this.
- **Callbacks upward.** `CCoreAudio` implements `IAudioSourceHelper`, so a decoder can
  push metadata it discovered (`UpdateMetaData(url, value, FIELD_*)`) and log messages;
  this is how streamed Shoutcast titles reach the library.
- `CCoreAudio` also implements `IMMNotificationClient` to react to the default audio
  device changing, and `IXAudio2EngineCallback` for critical-error restarts.

## 5. Media library and metadata

- `LibraryEntry` (`LibraryEntry.h`) is a flat POD struct — URL, kind, sizes, dates, play
  count, rating, the usual tag fields, replaygain values, bitrate/samplerate/channels.
  Older layouts are kept in the same header so the loader can migrate.
- `CMediaLibraryPlaylistEntry` wraps one `LibraryEntry` and implements `IPlaylistEntry`,
  exposing everything through `GetField(FIELD_*)` / `SetField` plus
  `GetTextRepresentation()` for display.
- `CMediaLibrary` owns `Array<CMediaLibraryPlaylistEntry*, 200>`, assigns entry IDs, and
  serialises to a versioned binary file (`TUNIAC_MEDIALIBRARY_VERSION`, currently 0.10) in
  the library folder. Lookups exist by index, entry ID and URL.
- Adding: `AddItem` dispatches to `AddFileToLibrary`, `AddDirectoryToLibrary` (recursive)
  or `AddStreamToLibrary`. A stream entry is stored with `ulKind = ENTRY_KIND_URL`,
  `ulPlaybackTime = LENGTH_STREAM` and file type `"Stream"` — this is the existing path a
  streaming service would reuse.
- Tags come from **info manager** plugins (`IInfoManager`, `CreateInfoManagerPlugin`),
  loaded by `CMediaLibrary` and matched by extension via `GetInfoManagerForFilename`.
  `std_InfoManager` is the real one and wraps taglib; it also supplies embedded album art.

## 6. Playlists

- `IPlaylist` is the basic contract (name, ID, navigation, active entry, entry lookup by
  filtered index). `IPlaylistEX` adds everything the UI needs: filtering, sorting, add /
  delete / move, entry-ID lookups, and the three index spaces the code keeps distinct —
  **real** index, **normal filtered** index and **random filtered** index (shuffle is a
  precomputed permutation, not a random pick).
- `CBasePlaylist` (40 KB) implements the shared behaviour; `CLibraryPlaylist`,
  `CStandardPlaylist` and `CAudioCDPlaylist` specialise it. `PLAYLIST_TYPE_*` and the
  `PLAYLIST_FLAGS_*` / `PLAYLISTEX_FLAGS_*` bits tell the UI what a playlist supports.
- `CPlaylistManager` owns the collection and the active playlist, persists the playlist
  database, and runs its own thread with a hidden window so it can receive
  `WM_DEVICECHANGE` and add/remove CD playlists as discs come and go.
- `CPlayQueue` and `CHistory` sit alongside as separate ordered lists; `CTuniacApp`
  consults them when computing "what plays next" and builds the Future menu from that.
- `FIELD_*` IDs are the lingua franca between playlists, the library, the UI columns and
  the decoders. They are `#define`d **twice** — in `IPlaylist.h` and again in
  `IAudioSource.h` — and the two lists must stay in sync.

## 7. User interface

Everything is hand-rolled Win32 with owner-drawn painting; there is no dialog framework.

- `CTuniacApp::WndProc` is the main window procedure and also the command router
  (`MENU_BASE`, `PREFERENCESMENU_BASE`, `HISTORYMENU_BASE`, `FUTUREMENU_BASE`,
  `TRAYMENU_BASE`, `PLAYLISTMENU_BASE` carve up the ID space).
- `CSourceSelectorWindow` is the left/right split view: a source tree on the left, and on
  the right whichever `ISourceView` is current — `CPlaylistSourceView` (the big one, 73 KB,
  a virtual list view with filtering, drag & drop, inline tag editing) or
  `CAudioCDSourceView`. `RadioSourceView` exists but is commented out of the build.
- `CPlayControls` (transport, seek, volume), `CAlbumArt` (art panel, `AlbumArt.cpp` decodes
  via libjpeg-turbo / libpng), `CVisualWindow` + `CTuniacVisual` (host for vis plugins),
  `CLogWindow` (the in-app console every subsystem logs to).
- `CTuniacSkin` + the `default-theme/`, `icons/`, `images/`, `cursors/` folders provide
  theming; the accent colour can follow the Windows accent.
- **Preferences** (`Preferences.cpp`, 110 KB) is a tree of pages: a `PrefPage` array pairs
  a name, a parent index, a `DLGTEMPLATE*` locked from the resource, and a `DLGPROC`. The
  tree is built from the parent links and the selected page is created with
  `CreateDialogIndirectParam`. `CPreferences` is also the settings store — typed getters
  and setters over `HKCU\Software\Tuniac`, plus `PluginGetValue` / `PluginSetValue`, which
  prefix a subkey path under the same root for plugin-owned settings.

## 8. The plugin system

Six families. They differ in interface and in where the DLL is found, but they all share
the same skeleton:

1. an exported factory `CreateXxx()` returning the interface pointer;
2. an exported `GetXxxVersion()` the host compares against a compile-time constant and
   refuses on mismatch;
3. an interface whose first method is `Destroy()` (the plugin does `delete this`);
4. a *helper* interface implemented by the host and handed to the plugin via
   `SetHelper()`, so callbacks never need the `tuniacApp` global across the DLL boundary.

| Family | Interface (header) | Loaded by | From | Factory export | Version guard |
|---|---|---|---|---|---|
| Decoder | `IAudioSourceSupplier` (`IAudioSource.h`) | `CCoreAudio::Startup` | EXE dir | `CreateDecoderPlugin` | `GetTuniacAudioSourceVersion` |
| Info manager | `IInfoManager` (`IInfoManager.h`) | `CMediaLibrary` | EXE dir | `CreateInfoManagerPlugin` | `GetTuniacInfoManagerVersion` |
| Import / export | `ITuniacImportPlugin`, `ITuniacExportPlugin` | `CImportExportManager` | `plugins\` | `CreateTuniacImportPlugin` / `…Export…` | `GetTuniacImportPluginVersion` / `…Export…` |
| General / notify | `ITuniacPlugin` (`ITuniacPlugin.h`) | `CPluginManager` | `plugins\` | `CreateTuniacPlugin` | `GetTuniacPluginVersion` |
| Visualisation | `ITuniacVisPlugin` | `CTuniacVisual` / `CVisualWindow` | `plugins\` | `CreateTuniacVisPlugin` | `GetTuniacVisPluginVersion` |
| **Service** **[branch]** | `ITuniacServicePlugin` | `CServicePluginManager` | `services\` | `CreateTuniacServicePlugin` | `GetTuniacPluginVersion` (see note) |

### General plugins in more detail

`CPluginManager` is the model the service manager was cloned from. `Initialize()`
enumerates `<exedir>\plugins\*.dll`, checks `GetTuniacPluginVersion() ==
ITUNIACPLUGIN_VERSION`, calls `CreateTuniacPlugin`, then reads the enable flag from
`HKCU\Software\Tuniac\plugins` **keyed by DLL file name**. A disabled plugin is destroyed
and unloaded immediately but *stays in the array* as a name-only entry so the preferences
list can still show it and re-enable it later. An enabled plugin gets `SetHelper(this)`
and its own low-priority thread via `ITuniacPlugin::CreateThread`; the app then broadcasts
`PLUGINNOTIFY_*` messages (song change, pause, play, seek, song played) to those threads
with `PostThreadMessage`. `CPluginManager` implements `ITuniacPluginHelper`, giving
plugins `GetVariable`, `GetTrackInfo` (format strings), `Navigate`, `LogMessage`, the main
`HINSTANCE`/`HWND`, and prefixed preference get/set.

`EnablePlugin(i, bool)` is the live toggle used by the preferences page: it loads or
unloads the DLL on the spot, and on disable politely posts `WM_QUIT` to the plugin's window
or thread before waiting, then terminating.

## 9. Cross-cutting utilities

| File | What it gives you |
|---|---|
| `Array.h` | The growable array template used everywhere (`Array<T, GrowBy>`), plus `IndexArray` / `EntryArray`. |
| `Singleton.h` | `CSingleton<T>`, used by `CCoreAudio`. |
| `CriticalSection.h` | RAII-ish wrapper over `CRITICAL_SECTION`. |
| `tuniacthread.h` | Thin thread helper. |
| `ringbuffer.h`, `DoubleBuffer.*`, `AudioPacketizer.*` | Buffering between decode and output. |
| `sfmt.*`, `Random.h` | SIMD-oriented Mersenne Twister for shuffle. |
| `SSE_Utils.*` | Small SSE helpers for mixing. |
| `generaldefs.h`, `Tuniac-Meta.h` | Shared constants, version/build metadata. |
| `PluginVariable.h` | The `Variable` enum plugins pass to `GetVariable`. |

## 10. Things to keep in mind when changing this code

- **The global is the API.** New subsystems get added as members of `CTuniacApp` and are
  reached as `tuniacApp.m_Thing`. That is the existing pattern, for better or worse.
- **Interface versions are load-bearing.** Editing an `ITuniac*` header without bumping its
  `ITUNIAC*_VERSION` silently ABI-breaks every shipped DLL of that family; bumping it
  locks out every DLL you do not rebuild.
- **Two field lists.** Add a `FIELD_*` to `IPlaylist.h` and you must add it to
  `IAudioSource.h` too, and probably to `FIELD_MAXFIELD`, the preferences column list and
  `CMediaLibraryPlaylistEntry`.
- **Registry is the settings store**; there is no config file. Plugin settings all land
  under one `plugins` subtree keyed by name, so names must stay unique across families.
- **Fixed-size buffers everywhere.** `TCHAR szFoo[64/128/256/MAX_PATH]` with `StringCch*`
  is the norm; sizes are frequently passed as element counts to one API and byte counts to
  another (`RegQueryValueEx` wants bytes). Check the units when copying an existing call.
- **No tests.** The only verification loop is building the solution and exercising the app.
