#!/usr/bin/env python3
"""Exercise the real Ring 3 shell through emulated keyboard IRQs."""
import json, pathlib, shutil, socket, subprocess, tempfile, time
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='os-shell-') as tmp:
    build = pathlib.Path(tmp)
    for name in ('boot', 'cpu', 'drivers', 'fs', 'kernel', 'libc', 'user'):
        shutil.copytree(root / name, build / name, ignore=shutil.ignore_patterns('*.o', '*.bin'))
    for name in ('Makefile', 'linker.ld'):
        shutil.copy(root / name, build / name)
    subprocess.run(['make', 'os-image.bin', 'CFLAGS=-g -ffreestanding -Wall -Wextra -fno-exceptions -m32 -fstack-protector-strong -std=gnu99 -DDEBUG_CONSOLE'], cwd=build, check=True, stdout=subprocess.DEVNULL)
    log, sock = build / 'debug.log', build / 'qmp.sock'
    process = subprocess.Popen(['qemu-system-i386', '-drive', 'file=os-image.bin,format=raw,if=floppy', '-display', 'none', '-serial', 'none', '-monitor', 'none', '-no-reboot', '-debugcon', f'file:{log}', '-qmp', f'unix:{sock},server=on,wait=off'], cwd=build)
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
                if 'return' in reply: return reply['return']
                assert 'error' not in reply, reply
        qmp('qmp_capabilities')
        def wait_for(text, offset=0):
            deadline = time.monotonic() + 8
            while True:
                output = log.read_text() if log.exists() else ''
                assert 'FAULT' not in output and 'PANIC' not in output, output
                if text in output[offset:]: return output[offset:]
                assert time.monotonic() < deadline, (text, output)
                time.sleep(.02)
        wait_for('user@os> ')
        for command, expected in [('help', 'Commands:'), ('pid', 'Current Shell Task PID: 1'), ('echo hello', '\nhello\nuser@os> '), ('touch note', 'created'), ('write note hello', 'written'), ('cat note', 'hello\n'), ('ls', 'note  (5 bytes)'), ('rm note', 'deleted'), ('cat note', 'cat failed')]:
            offset = len(log.read_text())
            for char in command + '\n':
                key = 'spc' if char == ' ' else 'ret' if char == '\n' else char
                qmp('human-monitor-command', {'command-line': f'sendkey {key} 10'})
                time.sleep(.025)
            output = wait_for('user@os> ', offset)
            assert expected in output, (command, output)
        print('shell: PASS (help pid echo touch write cat ls rm; keyboard IRQ)')
    finally:
        process.terminate()
        process.wait(timeout=5)
