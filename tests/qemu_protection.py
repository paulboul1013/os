#!/usr/bin/env python3
"""Build an isolated instrumented image and require a hardware test verdict."""
import pathlib, shutil, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='os-protection-') as tmp:
    build = pathlib.Path(tmp)
    for name in ('boot', 'cpu', 'drivers', 'fs', 'kernel', 'libc', 'user'):
        shutil.copytree(root / name, build / name, ignore=shutil.ignore_patterns('*.o', '*.bin'))
    for name in ('Makefile', 'linker.ld'):
        shutil.copy(root / name, build / name)
    subprocess.run(['make', 'os-image.bin', 'kernel.elf', 'CFLAGS=-g -ffreestanding -Wall -Wextra -fno-exceptions -m32 -fstack-protector-strong -std=gnu99 -DPROTECTION_TEST'], cwd=build, check=True, stdout=subprocess.DEVNULL)
    log = build / 'debug.log'
    result = subprocess.run(['qemu-system-i386', '-drive', 'file=os-image.bin,format=raw,if=floppy', '-display', 'none', '-serial', 'none', '-monitor', 'none', '-no-reboot', '-debugcon', f'file:{log}', '-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04'], cwd=build, timeout=35)
    output = log.read_text()
    print(output.splitlines()[-1] if result.returncode == 33 else output)
    if result.returncode != 33:
        shutil.copy(build / 'kernel.elf', '/tmp/os-failed-test.elf')
        shutil.copy(build / 'kernel.map', '/tmp/os-failed-test.map')
    assert result.returncode == 33 and 'PROTECTION TESTS PASS' in output
