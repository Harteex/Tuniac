# CLAUDE.md

Guidance for Claude Code (and humans) working in this repository.

## What this is

**Tuniac** is a Windows desktop music player written in C++ (raw Win32, no MFC/ATL, no
.NET). It is an old codebase — originally by Tony Million (2003–2008), then maintained
by Brett Hoyle and others — kept building on modern Visual Studio. Expect
C-with-classes: hand-rolled containers, `TCHAR` strings, Win32 message loops, manual
`new`/`delete`, `#define` constants rather than enums.

This checkout is a fork. `master` tracks upstream; **the `jellyfin2` branch is the fork
owner's own work** (branched from the earlier `jellyfin` branch), adding a *service
plugin* system with a Jellyfin streaming plugin as its first consumer.

## Repository layout

```
Tuniac/                     <- git root (this repo)
  Tuniac1/                  <- THE ACTIVE CODEBASE (Tuniac 1.x)
    TuniacApp.sln           <- the solution you build (VS 2022)
    TuniacApp/              <- the player EXE; all core code lives here
    bass_Plugin/            <- BASS decoder; handles almost all formats today
    ofr_Plugin/ shn_Plugin/ tak_Plugin/       <- extra decoders
    std_InfoManager/        <- tag reader/writer (taglib)
    m3u_Plugin/ pls_Plugin/                   <- playlist import/export plugins
    PopupNotify/ NowPlayingTxt/ mIRCSpammer/  <- notification plugins
    NFN_Exporter/ textOutput_Plugin/ MMShellHook/ MMShellHookHelper/
    SVPVisualPlugin/        <- visualisation plugin (Win32 only)
    Jellyfin_Plugin/        <- NEW on the jellyfin branches: the first service plugin
    Housekeeping/           <- build scripts, How_To_Build.txt, TODO.txt, changelog
    Guide/                  <- end-user HTML help
    Setup-Scripts/          <- Inno Setup installer scripts
  Tuniac2/                  <- abandoned rewrite experiment. Do not touch.
  Old/                      <- obsolete per-format decoders, superseded by BASS.
                               Not in the solution. Reference only.
  docs/                     <- analysis docs for this fork (see below)
```

Third-party libraries are **not** vendored — they are expected as sibling directories of
the repo root (`../zlib`, `../taglib`, `../libpng`, `../libjpeg-turbo`) and built first
via the scripts in `Tuniac1/Housekeeping/`. XAudio2 comes from the
`Microsoft.XAudio2.Redist` NuGet package under `Tuniac1/packages/`. The one exception is
RapidJSON, which *is* vendored, under `Tuniac1/Jellyfin_Plugin/rapidjson/`.

## Building

- Open `Tuniac1/TuniacApp.sln` in **Visual Studio 2022**. There is no CLI or CMake build,
  and no test suite. It cannot be built or run on a non-Windows host.
- Configurations `Debug` / `Release` / `Release_xp`; platforms `Win32` and `x64`.
  `Release_xp` uses the `v141_xp` toolset, everything else `v143`.
- Read `Tuniac1/Housekeeping/How_To_Build.txt` before the first build — the external libs
  must be built first or `TuniacApp` and `std_InfoManager` will not link.
- Output layout, which the runtime loaders depend on:

  | Artifact | Lands in | Loaded from |
  |---|---|---|
  | `Tuniac.exe` and decoder DLLs | `Tuniac1/$(Platform)/$(Configuration)/` | the EXE directory |
  | Notification / vis / import / export plugins | `.../plugins/` | `<exedir>\plugins\` |
  | Service plugins (new) | `.../services/` | `<exedir>\services\` |

## Architecture in one screen

`WinMain` (`Tuniac1/TuniacApp/Tuniac.cpp`) drives one global `CTuniacApp tuniacApp`:
`Initialize()` → `Run()` → `Shutdown()`. `CTuniacApp` is the god object — it owns the
media library, playlists, preferences, every window and the plugin managers, and
everything reaches everything else through that `tuniacApp` global.

- **Audio** — `CCoreAudio` (singleton) sets up XAudio2, scans the EXE directory for
  decoder DLLs, and keeps a short array of `CAudioStream`s so crossfade can overlap two
  tracks. Each `CAudioStream` owns one `IAudioSource` (the decoder), an EQ, replaygain
  scaling and a service thread that keeps its buffer filled.
- **Library** — `CMediaLibrary` holds `CMediaLibraryPlaylistEntry` objects wrapping a POD
  `LibraryEntry`, persisted to a versioned binary file. Tags come from `IInfoManager`
  plugins (`std_InfoManager`, taglib).
- **Playlists** — `IPlaylist` / `IPlaylistEX`, implemented by library, standard and
  audio-CD playlists, owned by `CPlaylistManager`.
- **UI** — hand-drawn Win32: `CSourceSelectorWindow` hosting `ISourceView`s, plus
  `CPlayControls`, `CAlbumArt`, `CVisualWindow`, `CLogWindow`, and a tree-driven
  `CPreferences` dialog.
- **Plugins** — six families, all the same shape: an exported `CreateXxx()` factory, an
  exported `GetXxxVersion()` guard, an interface whose first method is `Destroy()`, and a
  *helper* interface the host implements so the plugin can call back into the app.

Full detail: **[docs/architecture.md](docs/architecture.md)**.

## Conventions to follow when editing

- **Unicode `TCHAR` everywhere.** Wrap literals in `TEXT("…")`, use the `StringCch*`
  family from `strsafe.h`; never `strcpy`/`sprintf`. All projects build with `_UNICODE`.
- **Tabs** for indentation, Allman braces, `m_` for members, Hungarian-ish prefixes
  (`sz` string, `ul` unsigned long, `b` bool, `p` pointer, `h` handle).
- Interfaces are abstract classes named `IFoo`, all methods `= 0`, no virtual destructor —
  a plugin is released through its own `Destroy()`, which does `delete this`.
- `Array<T, N>` (`Array.h`) is the in-house growable array; `IndexArray` / `EntryArray`
  are the usual typedefs. The core uses no STL containers.
- Settings live in the registry under `HKCU\Software\Tuniac`; plugin settings go under
  `…\Tuniac\plugins\<name>` through `CPreferences::PluginGetValue` / `PluginSetValue`.
- Resource IDs are shared between `resource.h` and the `.rc`, which are maintained by the
  VS resource editor — keep diffs there tight.
- Bumping an interface version (`ITUNIAC*_VERSION`) invalidates every existing DLL of that
  family; the loader rejects mismatches with a message box.

## Current branch: `jellyfin2`

Working plan and checklist: **[docs/plan.md](docs/plan.md)**.

Goal: stream from a Jellyfin server through a **new, general plugin type** for streaming
services, rather than as a hardcoded special case.

Status: the plugin type works end to end. The Jellyfin plugin logs in and out from its
Configure dialog, and library entries holding `jellyfin://<serverid>/<itemid>` links play:
`CCoreAudio::SetSource` asks `CServicePluginManager` to resolve them into an authenticated
stream URL for BASS. What's missing is a way to get songs into the library other than
typing links (File → Import Stream…) — Phase 4 in [docs/plan.md](docs/plan.md).

[docs/plan.md](docs/plan.md) is the live checklist. [docs/service-plugins.md](docs/service-plugins.md)
is the original analysis, a snapshot as of `64b8b8c` — useful background, but several of its
"known issues" have since been fixed.

Things to know before touching this branch:

1. Service plugins keep settings under `HKCU\Software\Tuniac\services\…` (enable flags keyed
   by DLL name, plugin settings under `services\<name>`), separate from ordinary plugins.
2. A service link's resolved string (URL + auth header) must outlive the decoder that
   opened it; `CAudioStream` owns it. Don't pass a stack buffer to a decoder.
3. Line endings are mixed per file (newer files LF, older ones CRLF). Keep each file's
   existing endings so diffs stay small.