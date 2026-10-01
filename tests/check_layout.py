#!/usr/bin/env python3
"""Check page-aligned user boundaries in the actual linked ELF."""
import os, subprocess
symbols = {}
for line in subprocess.check_output([os.environ.get('NM', '/usr/local/i386elfgcc/bin/i386-elf-nm'), '-n', 'kernel.elf'], text=True).splitlines():
    parts = line.split()
    if len(parts) == 3:
        symbols[parts[2]] = int(parts[0], 16)
for name in ('__user_text_start', '__user_text_end', '__user_rodata_start', '__user_rodata_end', '__user_data_start', '__user_data_end'):
    assert name in symbols, f'missing user boundary: {name}'
    assert symbols[name] % 4096 == 0, name
assert symbols['__user_text_start'] <= symbols['user_shell_main'] < symbols['__user_text_end']
assert not symbols['__user_text_start'] <= symbols['kernel_main'] < symbols['__user_data_end']
assert symbols['__bss_end'] < 0x80000
print('layout: PASS')
