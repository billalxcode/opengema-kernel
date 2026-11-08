#!/usr/bin/env python3
"""Serial test client for QEMU.

Connects to the QEMU unix serial socket, mirrors output to a log, drives the
gemabox shell through help, acpi, clear, and halt, and stops at halt.

Usage: uart_test_client.py <socket> <log> [timeout_seconds] [gate_dir]
Exit nonzero on connect timeout or missing halt marker. When gate_dir is set,
the client waits for gate_dir/clear-continue after writing
gate_dir/clear-ready, so the caller can inspect the framebuffer.
"""

import os
import re
import socket
import sys
import time


def send_line(client, text):
    """Send a command slowly so the PL011 RX FIFO is always drained."""
    previous = client.gettimeout()
    client.settimeout(5.0)
    try:
        for value in text:
            client.sendall(bytes((value,)))
            time.sleep(0.01)
    finally:
        client.settimeout(previous)


def main(argv):
    if len(argv) < 3:
        print("usage: uart_test_client.py <socket> <log> [timeout]")
        return 2

    socket_path = argv[1]
    log_path = argv[2]
    timeout = float(argv[3]) if len(argv) > 3 else 120.0
    gate_dir = argv[4] if len(argv) > 4 else None
    deadline = time.time() + timeout

    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    while True:
        try:
            client.connect(socket_path)
            break
        except OSError:
            if time.time() >= deadline:
                print("uart client: connect timeout")
                return 1
            time.sleep(0.2)

    client.settimeout(0.5)
    buffer = bytearray()
    help_sent = False
    help_seen = False
    last_data = time.time()
    acpi_sent = False
    acpi_seen = False
    time_sent = False
    time_seen = False
    ps_sent = False
    ps_seen = False
    dmesg_sent = False
    dmesg_seen = False
    level_sent = False
    level_seen = False
    date_sent = False
    date_seen = False
    tzsave_sent = False
    tzsave_seen = False
    sysrq_sent = False
    sysrq_seen = False
    fs_sent = False
    fs_seen = False
    fs_batch = 0
    fs_output = bytearray()
    identity_sent = False
    identity_seen = False
    prio_cmd_sent = False
    prio_cmd_seen = False
    shell_sent = False
    shell_seen = False
    shell_batch = 0
    shell_batches = [
        b"pwd\ncd /tmp\npwd\ncd /\npwd\necho SHELL-1\n",
        b"touch /tmp/gemabox.txt\nstat /tmp/gemabox.txt\n"
        b"rm /tmp/gemabox.txt\necho SHELL-2\n",
        b"head /system/config/version\nwc /system/config/version\n"
        b"grep 0.1 /system/config/version\necho SHELL-3\n",
        b"pkg list\npkg info gemabox\necho PKG-DONE\necho SHELL-DONE\n",
    ]
    shell_markers = [b"SHELL-1", b"SHELL-2", b"SHELL-3", b"SHELL-DONE"]
    identity_batch = 0
    identity_output = bytearray()
    identity_batches = [
        b"chown 1 1 /tmp/hello\nstat /tmp/hello\nsetuid 1\nsetgid 1\nid\n",
        b"chmod 600 /tmp/writer\nsetuid 0\nsetgid 0\necho ID-DONE\n",
    ]
    fs_batches = [
        b"mkdir /tmp/d\nmv /tmp/d /tmp/e\nls /tmp\nrm /tmp/e\nstat /system/config/version\n",
        b"mkdir \"/tmp/quoted dir\"\nrm \"/tmp/quoted dir\"\nchmod 600 /tmp/hello\nstat /tmp/hello\n",
        b"mkdir /tmp/mnt\nmount ramfs /tmp/mnt\nmount ramfs /tmp/mnt\n",
        b"umount /tmp/mnt\numount /tmp/mnt\nrm /tmp/mnt\ncat /proc/meminfo\ncat /proc/stat\ncat /proc/threads\ncat /proc/syscalls\n",
        b"cat /proc/block\nls /volumes/boot\n",
        b"cat /proc/trace\n",
        b"cat /proc/devices\ncat /proc/interrupts\n",
        b"cat /proc/mounts\necho MOUNT-DONE\n",
    ]
    clear_sent = False
    clear_ready = False
    clear_released = False
    ls_sent = False
    ls_error_sent = False
    spawn_error_sent = False
    hello_sent = False
    fault_sent = False
    fault_seen = False
    wx_sent = False
    wx_seen = False
    nx_sent = False
    nx_seen = False
    badptr_sent = False
    badptr_seen = False
    fuzz_sent = False
    fuzz_seen = False
    mem_sent = False
    mem_seen = False
    errno_sent = False
    errno_seen = False
    fork_sent = False
    fork_seen = False
    exec_sent = False
    exec_seen = False
    lazy_sent = False
    lazy_seen = False
    shm_sent = False
    shm_seen = False
    unmap_sent = False
    unmap_seen = False
    futex_sent = False
    futex_seen = False
    tls_sent = False
    tls_seen = False
    filemap_sent = False
    filemap_seen = False
    prio_sent = False
    prio_seen = False
    aslr_sent = False
    aslr_first = None
    aslr_seen = False
    stack_sent = False
    stack_seen = False
    guard_sent = False
    guard_seen = False
    halt_sent = False
    halted = False

    with open(log_path, "wb") as log:
        while time.time() < deadline:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                chunk = None
            if chunk == b"":
                break
            if chunk:
                log.write(chunk)
                log.flush()
                buffer.extend(chunk)
                if fs_sent:
                    fs_output.extend(chunk)
                if identity_sent:
                    identity_output.extend(chunk)
                last_data = time.time()

            if not help_sent and b"gemabox> " in buffer:
                send_line(client, b"help\n")
                help_sent = True
                buffer.clear()

            if (help_sent and not help_seen and
                    b"commands: help, echo, cat, ls, pwd, cd, touch, head, wc, grep, free, stat, clear, acpi, uptime, date, tz, sleep, ps, prio, id, setuid, setgid, dmesg, loglevel, mkdir, rm, mv, chmod, chown, mount, umount, halt, shutdown, reboot, pkg" in buffer):
                help_seen = True

            if (help_seen and not acpi_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"acpi\n")
                acpi_sent = True
                buffer.clear()

            if acpi_sent and not acpi_seen and b"ACPI" in buffer:
                acpi_seen = True

            if (acpi_seen and not time_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"sleep 50\nuptime\n")
                time_sent = True
                buffer.clear()

            if time_sent and not time_seen and b"uptime: " in buffer:
                time_seen = True

            if (time_seen and not ps_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"ps\n")
                ps_sent = True
                buffer.clear()

            if ps_sent and not ps_seen and b"pid ppid" in buffer:
                ps_seen = True

            if (ps_seen and not dmesg_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"dmesg\n")
                dmesg_sent = True
                buffer.clear()

            if dmesg_sent and not dmesg_seen and b"dmesg:" in buffer:
                dmesg_seen = True

            if (dmesg_seen and not level_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"loglevel\nloglevel debug\nloglevel info\n")
                level_sent = True
                buffer.clear()

            if (level_sent and not level_seen and
                    b"loglevel: debug (3)" in buffer and
                    b"loglevel: info (2)" in buffer):
                level_seen = True

            if (level_seen and not date_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"date 1700000000\ndate\ntz 420\ndate\ntz 0\ndate\n")
                date_sent = True
                buffer.clear()

            if (date_sent and not date_seen and
                    b"2023-11-14 22:13:" in buffer and
                    b"2023-11-15 05:13:" in buffer and
                    b"UTC+07:00" in buffer):
                date_seen = True

            if (date_seen and not tzsave_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"tz 420\ntz save\ncat /volumes/boot/gsl.tz\ntz 0\necho TZSAVE-DONE\n")
                tzsave_sent = True
                buffer.clear()

            if (tzsave_sent and not tzsave_seen and
                    b"tz: saved" in buffer and b"TZSAVE-DONE" in buffer):
                tzsave_seen = True

            if (tzsave_seen and not sysrq_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"\x01s\n")
                sysrq_sent = True
                buffer.clear()

            if sysrq_sent and not sysrq_seen and b"sysrq: stats" in buffer:
                sysrq_seen = True

            if (sysrq_seen and not fs_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, fs_batches[0])
                fs_batch = 1
                fs_sent = True
                buffer.clear()

            if (fs_sent and not fs_seen and
                    fs_batch < len(fs_batches) and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, fs_batches[fs_batch])
                fs_batch += 1
                buffer.clear()

            if (fs_sent and not fs_seen and
                    fs_batch >= len(fs_batches) and
                    b"MOUNT-DONE" in fs_output):
                missing = []
                for marker in (b"e/", b"mode: 0644", b"mode: 0600",
                               b"mount: mounted", b"umount: unmounted",
                               b"virtio-blk1", b"frames_free:",
                               b"dma_allocs:", b"context_switches:", b"vfs_ops:",
                               b"block_reads:", b"context_switch ", b"pl031",
                               b"irq count", b"path refs"):
                    if marker not in fs_output:
                        missing.append(marker.decode())
                if missing or b"mount: failed" in fs_output or \
                        b"umount: failed" in fs_output or \
                        b"rm: failed" in fs_output:
                    print("uart client: mount test failed: " +
                          (", ".join(missing) if missing else "command failed"))
                    return 1
                fs_seen = True

            if (fs_seen and not identity_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, identity_batches[0])
                identity_batch = 1
                identity_sent = True
                buffer.clear()

            if (identity_sent and not identity_seen and
                    identity_batch < len(identity_batches) and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, identity_batches[identity_batch])
                identity_batch += 1
                buffer.clear()

            if (identity_sent and not identity_seen and
                    identity_batch >= len(identity_batches) and
                    b"ID-DONE" in identity_output):
                missing = []
                for marker in (b"uid: 1", b"gid: 1", b"chmod: failed",
                               b"setuid: failed", b"setgid: failed"):
                    if marker not in identity_output:
                        missing.append(marker.decode())
                if missing or b"chown: failed" in identity_output:
                    print("uart client: identity test failed: " +
                          (", ".join(missing) if missing else "chown failed"))
                    return 1
                identity_seen = True

            if (identity_seen and not prio_cmd_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"prio\necho PRIO-CMD-DONE\n")
                prio_cmd_sent = True
                buffer.clear()

            if prio_cmd_sent and not prio_cmd_seen and \
                    b"PRIO-CMD-DONE" in buffer:
                prio_cmd_seen = True

            if (prio_cmd_seen and not shell_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, shell_batches[0])
                shell_batch = 1
                shell_sent = True
                buffer.clear()

            if (shell_sent and not shell_seen and
                    shell_batch < len(shell_batches) and
                    shell_markers[shell_batch - 1] in buffer):
                send_line(client, shell_batches[shell_batch])
                shell_batch += 1
                buffer.clear()

            if shell_sent and not shell_seen and b"SHELL-DONE" in buffer:
                shell_seen = True

            if (shell_seen and not clear_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"clear\necho CLEAR-DONE\n")
                clear_sent = True
                buffer.clear()

            if (clear_sent and not clear_ready and
                    b"CLEAR-DONE" in buffer and
                    time.time() - last_data > 0.2):
                if gate_dir is not None:
                    open(os.path.join(gate_dir, "clear-ready"), "w").close()
                clear_ready = True

            if clear_ready and not clear_released:
                if gate_dir is None or os.path.exists(
                        os.path.join(gate_dir, "clear-continue")):
                    clear_released = True

            if clear_released and not ls_sent:
                send_line(client, b"ls /\n")
                ls_sent = True

            if ls_sent and not ls_error_sent and b"system/" in buffer:
                send_line(client, b"ls /missing\n")
                ls_error_sent = True

            if ls_error_sent and not spawn_error_sent and b"ls: list failed" in buffer:
                send_line(client, b".\n")
                spawn_error_sent = True

            if spawn_error_sent and not hello_sent and b"run: spawn failed" in buffer:
                send_line(client, b"hello\n")
                hello_sent = True

            if hello_sent and not fault_sent and b"hello from applications" in buffer:
                send_line(client, b"hello fault\necho FAULT-DONE\n")
                fault_sent = True
                buffer.clear()

            if fault_sent and not fault_seen and b"FAULT-DONE" in buffer:
                fault_seen = True

            if fault_seen and not wx_sent and b"FAULT-DONE" in buffer:
                send_line(client, b"hello wx\necho WX-DONE\n")
                wx_sent = True
                buffer.clear()

            if wx_sent and not wx_seen and b"WX-DONE" in buffer:
                wx_seen = True

            if wx_seen and not nx_sent and b"WX-DONE" in buffer:
                send_line(client, b"hello nx\necho NX-DONE\n")
                nx_sent = True
                buffer.clear()

            if nx_sent and not nx_seen and b"NX-DONE" in buffer:
                nx_seen = True

            if nx_seen and not badptr_sent and b"NX-DONE" in buffer:
                send_line(client, b"hello badptr\necho BADPTR-DONE\n")
                badptr_sent = True
                buffer.clear()

            if badptr_sent and not badptr_seen and b"BADPTR-DONE" in buffer:
                badptr_seen = True

            if badptr_seen and not fuzz_sent and b"BADPTR-DONE" in buffer:
                send_line(client, b"hello fuzz\necho FUZZ-DONE\n")
                fuzz_sent = True
                buffer.clear()

            if fuzz_sent and not fuzz_seen and b"FUZZ-DONE" in buffer:
                fuzz_seen = True

            if fuzz_seen and not mem_sent and b"FUZZ-DONE" in buffer:
                send_line(client, b"hello mem\necho MEM-DONE\n")
                mem_sent = True
                buffer.clear()

            if mem_sent and not mem_seen and b"MEM-DONE" in buffer:
                mem_seen = True

            if mem_seen and not errno_sent and b"MEM-DONE" in buffer:
                send_line(client, b"hello errno\necho ERRNO-DONE\n")
                errno_sent = True
                buffer.clear()

            if errno_sent and not errno_seen and b"ERRNO-DONE" in buffer:
                errno_seen = True

            if errno_seen and not fork_sent and b"ERRNO-DONE" in buffer:
                send_line(client, b"hello fork\necho FORK-DONE\n")
                fork_sent = True
                buffer.clear()

            if fork_sent and not fork_seen and b"FORK-DONE" in buffer:
                fork_seen = True

            if fork_seen and not exec_sent and b"FORK-DONE" in buffer:
                send_line(client, b"hello exec\necho EXEC-DONE\n")
                exec_sent = True
                buffer.clear()

            if exec_sent and not exec_seen and b"EXEC-DONE" in buffer:
                exec_seen = True

            if exec_seen and not lazy_sent and b"EXEC-DONE" in buffer:
                send_line(client, b"hello lazy\necho LAZY-DONE\ncat /proc/meminfo\n")
                lazy_sent = True
                buffer.clear()

            if lazy_sent and not lazy_seen and b"LAZY-DONE" in buffer:
                lazy_seen = True

            if lazy_seen and not shm_sent and b"LAZY-DONE" in buffer:
                send_line(client, b"hello shm\necho SHM-DONE\n")
                shm_sent = True
                buffer.clear()

            if shm_sent and not shm_seen and b"SHM-DONE" in buffer:
                shm_seen = True

            if shm_seen and not unmap_sent and b"SHM-DONE" in buffer:
                send_line(client, b"hello unmap\necho UNMAP-DONE\n")
                unmap_sent = True
                buffer.clear()

            if unmap_sent and not unmap_seen and b"UNMAP-DONE" in buffer:
                unmap_seen = True

            if unmap_seen and not futex_sent and b"UNMAP-DONE" in buffer:
                send_line(client, b"hello futex\necho FUTEX-DONE\n")
                futex_sent = True
                buffer.clear()

            if futex_sent and not futex_seen and b"FUTEX-DONE" in buffer:
                futex_seen = True

            if futex_seen and not tls_sent and b"FUTEX-DONE" in buffer:
                send_line(client, b"hello tls\necho TLS-DONE\n")
                tls_sent = True
                buffer.clear()

            if tls_sent and not tls_seen and b"TLS-DONE" in buffer:
                tls_seen = True

            if tls_seen and not filemap_sent and b"TLS-DONE" in buffer:
                send_line(client, b"hello filemap\necho FILEMAP-DONE\n")
                filemap_sent = True
                buffer.clear()

            if filemap_sent and not filemap_seen and b"FILEMAP-DONE" in buffer:
                filemap_seen = True

            if filemap_seen and not prio_sent and b"FILEMAP-DONE" in buffer:
                send_line(client, b"hello prio\necho PRIO-DONE\n")
                prio_sent = True
                buffer.clear()

            if prio_sent and not prio_seen and b"PRIO-DONE" in buffer:
                prio_seen = True

            if prio_seen and not aslr_sent and b"PRIO-DONE" in buffer:
                send_line(client, b"hello aslr\necho ASLR-1\n")
                aslr_sent = True
                buffer.clear()

            if aslr_sent and aslr_first is None and b"ASLR-1" in buffer:
                match = re.search(
                    rb"aslr: stack (\d+) mmap (\d+) code (\d+)", buffer)
                if match is None:
                    print("uart client: ASLR first run missing")
                    return 1
                aslr_first = (int(match.group(1)), int(match.group(2)),
                              int(match.group(3)))
                send_line(client, b"hello aslr\necho ASLR-2\n")
                buffer.clear()

            if (aslr_sent and aslr_first is not None and not aslr_seen and
                    b"ASLR-2" in buffer):
                match = re.search(
                    rb"aslr: stack (\d+) mmap (\d+) code (\d+)", buffer)
                if match is None:
                    print("uart client: ASLR second run missing")
                    return 1
                second = (int(match.group(1)), int(match.group(2)),
                          int(match.group(3)))
                if second == aslr_first:
                    print("uart client: ASLR did not vary")
                    return 1
                aslr_seen = True

            if aslr_seen and not stack_sent and b"ASLR-2" in buffer:
                send_line(client, b"hello stack\necho STACK-DONE\n")
                stack_sent = True
                buffer.clear()

            if stack_sent and not stack_seen and b"STACK-DONE" in buffer:
                stack_seen = True

            if stack_seen and not guard_sent and b"STACK-DONE" in buffer:
                send_line(client, b"hello stackguard\necho STACKGUARD-DONE\n")
                guard_sent = True
                buffer.clear()

            if guard_sent and not guard_seen and b"STACKGUARD-DONE" in buffer:
                guard_seen = True

            if guard_seen and not halt_sent and b"STACKGUARD-DONE" in buffer:
                send_line(client, b"halt\n")
                halt_sent = True

            if b"kernel: halt" in buffer:
                halted = True
                break

    client.close()
    if not halted:
        print("uart client: halt marker not seen")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
