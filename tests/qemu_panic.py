#!/usr/bin/env python3
"""A supervisor write to read-only user text must panic and remain halted."""
import pathlib, shutil, subprocess, tempfile, time
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='os-panic-') as tmp:
    build = pathlib.Path(tmp)
    for name in ('boot', 'cpu', 'drivers', 'fs', 'kernel', 'libc', 'user'):
        shutil.copytree(root / name, build / name, ignore=shutil.ignore_patterns('*.o', '*.bin'))
    for name in ('Makefile', 'linker.ld'):
        shutil.copy(root / name, build / name)
    subprocess.run(['make', 'os-image.bin', 'CFLAGS=-g -ffreestanding -Wall -Wextra -fno-exceptions -m32 -fstack-protector-strong -std=gnu99 -DPROTECTION_PANIC -DDEBUG_CONSOLE'], cwd=build, check=True, stdout=subprocess.DEVNULL)
    log = build / 'debug.log'
    process = subprocess.Popen(['qemu-system-i386', '-drive', 'file=os-image.bin,format=raw,if=floppy', '-display', 'none', '-serial', 'none', '-monitor', 'none', '-no-reboot', '-debugcon', f'file:{log}'], cwd=build)
    try:
        deadline = time.monotonic() + 8
        while True:
            output = log.read_text() if log.exists() else ''
            if 'KERNEL PANIC' in output: break
            assert time.monotonic() < deadline, output
            time.sleep(.02)
        time.sleep(.2)
        output = log.read_text()
        assert process.poll() is None, 'QEMU exited after kernel fault'
        assert output.count('KERNEL PANIC') == 1, output
        assert 'vector=14' in output and 'error=0x3' in output, output
        assert 'USER FAULT' not in output and 'user@os>' not in output, output
        print('kernel panic: PASS (supervisor write, CR0.WP, no recovery)')
    finally:
        process.terminate()
        process.wait(timeout=5)
