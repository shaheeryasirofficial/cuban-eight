# Cuban Eight

![Cuban Eight](cuban-eight/cuban.png)

WSL Distro Packager for Red Team Research

**Author:** Maverick  
**GitHub:** [https://github.com/shaheeryasirofficial](https://github.com/shaheeryasirofficial)

---

## Overview

Cuban Eight takes a Windows PE payload and packages it as a `.wsl` file  the same double-click installer format used by real WSL distros. When the target opens the file, Windows registers it as a Linux distribution and immediately executes the payload through WSL's PE interop layer.

No elevation required. No script execution policy. No obvious `.exe` on disk. Just a file that looks like a Linux distro installer.

The packaged distro spoofs a real distribution identity: Ubuntu, Debian, Kali, openSUSE, or Fedora.

---

## Demo

![Demo](cuban-eight/beacon.png)

The beacon connects back within seconds of the target double-clicking the `.wsl` file. The WSL window that briefly appears closes automatically once the payload runs.

**Known behavior on first launch:**

```
wsl: Failed to mount C:\, see dmesg for more details.
wsl: Failed to mount D:\, see dmesg for more details.
wsl: Failed to mount E:\, see dmesg for more details.
wsl: Failed to mount F:\, see dmesg for more details.
wsl: Failed to mount G:\, see dmesg for more details.
wsl: Failed to mount H:\, see dmesg for more details.
wsl: Failed to translate 'C:\Users\USER\Desktop'
Press any key to exit...
```

These mount warnings appear briefly during the first launch and are cosmetically undesirable. They occur because `wsl.conf` (which suppresses drive mounting) only takes effect after the OOBE phase completes  WSL processes it too late to suppress first-launch output. Silencing this window on first launch is on the roadmap.

---

## How It Works

WSL's `.wsl` format is a plain uncompressed tar archive of a Linux root filesystem. Windows reads the `wsl-distribution.conf` inside it to register the distro name and then unpacks and launches it.

Cuban Eight builds a minimal scratch rootfs (~18KB) with no real Linux binaries. The payload is placed at `/root/<name>.exe`. The `/etc/passwd` root entry points directly at that path as the login shell.

When WSL launches the distro and logs in as root, the kernel sees an MZ executable in the shell field, hands it to `binfmt_misc`, and WSL's interop layer calls Windows `CreateProcess` on it. The payload runs as a native Windows process.

Key files in the rootfs:

`/etc/wsl-distribution.conf`  registers the distro name and sets root as default user  
`/etc/wsl.conf`  suppresses Windows drive mount noise  
`/etc/passwd`  points root's shell directly at the payload  
`/root/<payload>.exe`  the actual PE binary

No `/bin/sh`. No shell scripts. No interpreter in the rootfs at all.

---

## Project Structure

```
cuban-eight/
    cuban-eight.py      Python frontend (interactive menu)
    build.sh            Compiles the C backend
    c/
        cuban8_pack.c   C backend (builds and writes the tar)
        cuban8-pack     Compiled binary (after build)
```

---

## Requirements

Python 3.6 or later  
GCC  
Linux or WSL build environment

---

## Build

```bash
chmod +x build.sh
./build.sh
```

This compiles `c/cuban8_pack.c` and outputs `c/cuban8-pack`.

---

## Usage

```bash
python3 cuban-eight.py
```

The tool walks through:

1. Distro identity selection
2. Path to the PE payload
3. Internal payload filename
4. Output path for the `.wsl` file

The resulting `.wsl` file can be delivered to a target and opened with a double-click.

---

## Supported Distro Identities

| ID          | Spoofed Name            |
|-------------|-------------------------|
| ubuntu2204  | Ubuntu 22.04.3 LTS      |
| ubuntu2004  | Ubuntu 20.04.6 LTS      |
| debian      | Debian GNU/Linux        |
| kali        | Kali Linux Rolling      |
| opensuse    | openSUSE Leap 15.5      |
| fedora      | Fedora Linux 39         |

---

## Delivery

The `.wsl` file can be delivered through any channel that preserves the file extension: phishing email attachment, USB drop, shared drive, web download.

On Windows 10 (build 19041+) and Windows 11 with WSL installed, double-clicking a `.wsl` file triggers the install flow without any additional prompts.

If the target already has a distro registered under the same name, the install will fail with `ERROR_ALREADY_EXISTS`. Use a less common distro ID in that case.

---

## Detection Notes

The rootfs contains no standard Linux binaries, no shell, and no scripts. Static analysis of the tar will find only the payload and a handful of config files.

The distro name appears in the Windows registry under `HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss` after installation. Cleanup: `wsl --unregister <distro-name>`.

EDR coverage of WSL-launched processes varies. The payload runs as a child of `wslhost.exe` or `wsl.exe` depending on Windows version.

---

## Research Background

The WSL `.wsl` format was documented through black-box testing. Key findings:

The format is a plain uncompressed tar, not gzip or ZIP. `wsl-distribution.conf` with a `[oobe]` section is required for double-click registration. The `defaultName` field sets the registered distro name.

WSL's `binfmt_misc` registration intercepts `execv` calls on MZ binaries inside the WSL filesystem and routes them to Windows `CreateProcess` via the interop layer. This is what makes the passwd shell trick work  no Linux interpreter is needed.

Prior public research used Alpine Linux as a base (~8MB). Cuban Eight builds a scratch rootfs from scratch at ~18KB.

---

## Disclaimer

Cuban Eight is a research tool. Use only in authorized engagements or controlled lab environments. Unauthorized use against systems you do not own or have explicit permission to test is illegal.
