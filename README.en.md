# KeyKey 41 — Eten Tribute Edition

[繁體中文](README.md) | [English](README.en.md)

![Windows 11](https://img.shields.io/badge/Windows%2011-x64%20%2B%20x86-0078D6?logo=windows)
![Version](https://img.shields.io/badge/version-0.9.4--beta.1-B45DB7)
![License](https://img.shields.io/badge/license-MIT-green)

KeyKey 41 is a Traditional Chinese Bopomofo input method for Windows 11. It
recreates interaction patterns familiar to Yahoo! KeyKey users on the modern
Windows TSF architecture while preserving Eten 41-key and standard Bopomofo
keyboard layouts.

> This is an independent community tribute. It is not affiliated with, licensed
> by, or endorsed by Yahoo or Yahoo! Taiwan. “Yahoo” and “Yahoo!” are trademarks
> of their respective owners.

## Problem

People who rely on Yahoo KeyKey or the Eten 41-key layout have few maintained
options that preserve their familiar workflow on current Windows systems.
Legacy input-method architectures also do not directly meet the compatibility
needs of Windows 11, 64-bit applications, and 32-bit/WOW64 applications.

KeyKey 41 rebuilds that experience as a Windows TSF text service:

- Eten 41-key and standard Bopomofo layouts;
- intelligent Bopomofo composition and candidate selection;
- Traditional Chinese, Simplified Chinese, and English modes;
- Yahoo KeyKey-style Shift/Ctrl punctuation and symbol behaviour;
- configurable phrases, shortcuts, candidate direction, font, and colours; and
- both native x64 and real x86/WOW64 input-method components.

## Demo

The screenshot below shows the current preferences application:

![KeyKey 41 preferences](docs/images/偏好設定.jpg)

## Before / After

| Before: a gap on modern Windows | After: KeyKey 41 |
|---|---|
| No maintained equivalent of the familiar Yahoo KeyKey/Eten 41-key workflow | Preserves keyboard layout, candidate selection, and shortcut habits |
| Legacy input-method components do not match current Windows architecture | Reimplemented as a Windows TSF text service |
| 64-bit and 32-bit applications require different TIP architectures | One MSI installs both x64 and x86/WOW64 components |
| Candidate appearance and key behaviour are difficult to customise | Configure direction, font, colours, shortcuts, and user phrases |

## Installation

1. Download the latest MSI from
   [Releases](https://github.com/whyren0324/KeyKey41-Eten-Tribute/releases/latest).
   The current test release is `0.9.4-beta.1`.
2. Compare the file's SHA-256 checksum with the release notes.
3. Run the MSI with administrator privileges.
4. Select **KeyKey 41** from the Windows language and input-method menu.

The default location on 64-bit Windows is:

```text
C:\Program Files\KeyKey41\EtenTribute\
```

> The MSI is not currently code-signed, so Windows SmartScreen may display a
> warning. Only download it from this repository and verify its checksum.

KeyKey 41 is a system input method that requires TSF/COM registration. It is
therefore not distributed as a supposedly “no-install” portable application.

## Usage

### Open preferences

1. Switch to the **KeyKey 41** input method.
2. Find the KeyKey 41 input-method icon in the Windows taskbar.
3. Right-click the icon.
4. Select **Settings**.

Changes are saved automatically. If an already-open application has not picked
up new settings, select **Reload** or switch away from and back to KeyKey 41.

### Common shortcuts

| Action | Default shortcut |
|---|---|
| Toggle Chinese/English | Left Shift |
| Toggle Traditional/Simplified Chinese | Ctrl + F3 |
| Toggle full-width/half-width | Shift + Space |
| Full-width comma `，` | Right Shift + `,` |
| Full-width period `。` | Right Shift + `.` |
| Left double quotation mark `『` | Right Shift + `[` |
| Right double quotation mark `』` | Right Shift + `]` |

Actual behaviour can be changed in preferences. See the
[feature coverage document](docs/keykey-feature-coverage.md) for details.

## Build from source

Requirements:

- Visual Studio 2022 with Desktop development with C++ and x64/x86 tools
- Windows 10/11 SDK
- CMake 3.20 or later
- WiX Toolset 7 with UI and Util extensions

```powershell
git clone --recurse-submodules https://github.com/whyren0324/KeyKey41-Eten-Tribute.git
cd KeyKey41-Eten-Tribute
.\build_msi.ps1
```

The MSI is written to `dist\`. See the
[development documentation](docs/README.md) and
[installer guide](docs/installer.md) for more detail.

## Roadmap

- Complete Windows 11 x64 and x86/WOW64 release validation on the path to 1.0.
- Improve compatibility with older Win32 software, Office dialogs, elevated
  applications, and unusual text fields.
- Establish a native ARM64 build and hardware-validation path.
- Evaluate code signing to reduce SmartScreen and installation trust issues.
- Improve installation, upgrade, removal, and version-migration behaviour.

Roadmap items are planned directions, not committed release dates. Because a
Windows input method requires system registration, a truly no-install portable
edition is not currently planned.

## Todo

- [ ] Complete x64 and x86 compatibility items in the release checklist.
- [ ] Test more Office, Chromium, and legacy Win32 text fields.
- [ ] Establish a native ARM64 build and test environment.
- [ ] Automatically verify EXE, DLL, and MSI version metadata.
- [ ] Evaluate an affordable code-signing option.

See the full
[Windows compatibility and release checklist](docs/windows-compatibility-release-checklist.md).

## Credits and license

This project references or incorporates public work from
[win-mcbopomofo](https://github.com/openvanilla/win-mcbopomofo),
[McBopomofo](https://github.com/openvanilla/McBopomofo), and
[YahooArchive/KeyKey](https://github.com/YahooArchive/KeyKey). Third-party
components remain subject to their original licences.

Released under the [MIT License](LICENSE.txt).
