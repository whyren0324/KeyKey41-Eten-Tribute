# Win-McBopomofo Installer Guide

This document describes what the Win-McBopomofo MSI installer does when run on a user's machine, and explains how developers can build it.

## 1. What the Installer Does

**Note: Running the MSI installer requires elevated (Administrator) privileges.** This is because it writes to protected system directories (`Program Files`) and performs system-wide COM registration.

The installer is authored using the WiX Toolset v4/v7 (`installer/installer.wxs`) and performs the following actions during installation:

### A. Pre-requisite Checks and Cleanup

- Checks if `McBopomofoServer.exe` or `McBopomofoConfig.exe` is currently running and attempts to close them gracefully.
- Runs a custom VBScript (`scripts\CheckAndCloseAppsWithTIP.vbs`) to detect and close applications that currently have the TSF DLL loaded. This helps prevent file locks that require a system reboot.

### B. File Deployment

The KeyKey 41 AMD64 package installs all files under:
`C:\Program Files\KeyKey41\EtenTribute`.

The x86 compatibility TIP remains in this common directory. Its PE architecture
and 32-bit COM registry view—not the directory name—allow WOW64 applications to
load it.

The files deployed include:

- Core Executables:
    - `McBopomofoServer_<arch>.exe`
    - `McBopomofoConfig_<arch>.exe`
- The TSF Client DLL: `McBopomofoTIP_<arch>.dll`
- Language Models & Data (`data\`): `data.txt`, `data-plain-bpmf.txt`, `dictionary_service.json`, etc.
- OpenCC Conversion Data (`data\opencc\`): Contains `.ocd2` dictionaries and `tw2s.json`.

On AMD64 Windows the package contains both a native x64 TIP and a real Win32
x86 TIP. Both clients share one x64 server through the named pipe. ARM64 must be
shipped as a separately compiled and validated native package.

### C. System Registration & Configuration

- **COM Registration (TSF):** On AMD64 Windows the installer registers both
  `McBopomofoTIP_x86.dll` with 32-bit `regsvr32` and
  `McBopomofoTIP_x64.dll` with 64-bit `regsvr32`. The x86 registration runs
  first and x64 registration runs last.
- **Autorun:** Adds a registry string named `Win-McBopomofo-Server` pointing to the Server executable in `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`. This ensures the background engine starts when the user logs in.
- **Start Menu Shortcut:** Creates a shortcut in the Start Menu for the configuration app (`McBopomofoConfig_<arch>.exe`).

### D. Post-Installation

- Executes the architecture-appropriate `McBopomofoServer.exe` immediately after the installation finishes, ensuring the user can type right away without logging out.

---

## 2. Uninstallation Process

When the user uninstalls the software, the MSI reverses the process:

1. Closes active processes.
2. Unregisters both x86 and x64 TSF Client DLLs from their corresponding COM registry views.
3. Removes the Start Menu shortcuts and Autorun registry keys.
4. Deletes the deployed binaries and data files.

---

## 3. How to Build the Installer

Building the MSI installer is automated via the `build_msi.ps1` script at the root of the repository.

### Prerequisites

Make sure you have satisfied the [Development Requirements](../README.md#development-requirements) (Visual Studio with C++ tools for x86/x64/ARM64, CMake, and WiX Toolset).

### Build Command

Open a PowerShell terminal at the project root and run:

```powershell
.\build_msi.ps1
```

### What the Script Does

1. **Compiles AMD64 compatibility targets:** It builds native x64 and Win32/x86
   artifacts. ARM64 is not claimed until it has a separate native build and
   validation path.
2. **Generates OpenCC dictionaries:** Triggers the OpenCC dictionary build target.
3. **Converts the License:** Generates an RTF version of `LICENSE.txt` to embed in the MSI wizard UI.
4. **Executes WiX:** Invokes `wix build` with the `installer/installer.wxs` file, passing the build output paths (e.g., `X64BinDir=build_x64\bin\Release`) as bind variables.

### Customizing the Build

The script supports several flags:

- **Build Debug MSI:**

  ```powershell
  .\build_msi.ps1 -Configuration Debug
  ```

- **Skip Binary Build (If you have already built them):**

  ```powershell
  .\build_msi.ps1 -SkipBuild
  ```

- **Custom Output Name:**

  ```powershell
  .\build_msi.ps1 -OutputName "MyCustomBuild.msi"
  ```

### Outputs

Once complete, the final installer is placed in the `dist\` directory:

- `dist\Win-McBopomofo-Installer.msi`
