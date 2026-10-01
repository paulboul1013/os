#!/usr/bin/env python3
"""Build an isolated scheduling image and require its QEMU verdict."""
import pathlib
import json
import shutil
import socket
import subprocess
import tempfile
import time

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='os-scheduling-') as tmp:
    build = pathlib.Path(tmp)
    for name in ('boot', 'cpu', 'drivers', 'fs', 'kernel', 'libc', 'user'):
        shutil.copytree(root / name, build / name, ignore=shutil.ignore_patterns('*.o', '*.bin'))
    for name in ('Makefile', 'linker.ld'):
        shutil.copy(root / name, build / name)
    flags = '-g -ffreestanding -Wall -Wextra -fno-exceptions -m32 -fstack-protector-strong -std=gnu99 -DSCHED_TEST'
    subprocess.run(['make', 'os-image.bin', f'CFLAGS={flags}'], cwd=build,
                   check=True, stdout=subprocess.DEVNULL)
    assert (build / 'kernel.bin').stat().st_size <= 128 * 512
    log, sock = build / 'debug.log', build / 'qmp.sock'
    process = subprocess.Popen([
        'qemu-system-i386', '-drive', 'file=os-image.bin,format=raw,if=floppy',
        '-display', 'none', '-serial', 'none', '-monitor', 'none', '-no-reboot',
        '-debugcon', f'file:{log}', '-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04',
        '-qmp', f'unix:{sock},server=on,wait=off',
    ], cwd=build)
    try:
        deadline = time.monotonic() + 10
        while not sock.exists():
            assert process.poll() is None, 'QEMU exited during startup'
            assert time.monotonic() < deadline, 'QMP startup timeout'
            time.sleep(.02)
        connection = socket.socket(socket.AF_UNIX)
        connection.settimeout(5)
        connection.connect(str(sock))
        stream = connection.makefile('rwb', buffering=0)
        stream.readline()

        def qmp(command, arguments=None):
            stream.write((json.dumps({'execute': command, 'arguments': arguments or {}}) + '\n').encode())
            while True:
                reply = json.loads(stream.readline())
                if 'return' in reply:
                    return reply['return']
                assert 'error' not in reply, reply

        def wait_for(marker):
            deadline = time.monotonic() + 10
            while True:
                output = log.read_text() if log.exists() else ''
                assert 'FAULT' not in output and 'PANIC' not in output, output
                if marker in output:
                    return
                assert process.poll() is None, output
                assert time.monotonic() < deadline, (marker, output)
                time.sleep(.02)

        def type_line(line):
            for char in line + '\n':
                key = 'ret' if char == '\n' else 'spc' if char == ' ' else char
                qmp('human-monitor-command', {'command-line': f'sendkey {key} 10'})
                time.sleep(.025)

        qmp('qmp_capabilities')
        wait_for('SCHED READ WAIT')
        time.sleep(.12)
        type_line('wake')
        wait_for('SCHED EARLY INPUT')
        type_line('early')
        wait_for('SCHED SLOT FULL INPUT')
        type_line('first')
        type_line('second')
        wait_for('SCHED EMPTY INPUT')
        type_line('')
        wait_for('SCHED IRQ PENDING WINDOW')
        type_line('')
        wait_for('SCHED COPY FAILURE INPUT')
        type_line('bad')
        wait_for('SCHED SHELL BACKGROUND PASS')
        type_line('echo scheduling')
        wait_for('scheduling\nuser@os> ')
        type_line('exit')
        try:
            result = process.wait(timeout=10)
        except subprocess.TimeoutExpired as error:
            raise AssertionError(log.read_text()) from error
        output = log.read_text()
        print(output.splitlines()[-1] if result == 33 else output)
        assert result == 33 and 'SCHED TESTS PASS' in output
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
