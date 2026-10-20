import subprocess
import time
import socket

# Start QEMU with serial stdio and telnet monitor
proc = subprocess.Popen([
    'qemu-system-i386', '-m', '128M', '-kernel', 'kernel.akl',
    '-initrd', 'isodir/apps/shell.bin,isodir/apps/hello.bin,isodir/apps/downloadfile.bin',
    '-netdev', 'user,id=net0', '-device', 'e1000,netdev=net0',
    '-serial', 'stdio', '-monitor', 'tcp:127.0.0.1:4444,server,nowait', '-display', 'none'
], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

time.sleep(5.0) # Wait for boot and DHCP

# Send command
proc.stdin.write(b"downloadfile -web http://example.com/\n")
proc.stdin.flush()

# Wait 8s for download to finish and hang
time.sleep(8.0)

# Connect to QEMU monitor
s = socket.socket()
s.connect(('127.0.0.1', 4444))
time.sleep(0.5)
s.sendall(b"info registers\n")
time.sleep(0.5)
data = s.recv(4096)
print("=== CPU REGISTERS AT HANG ===")
print(data.decode(errors='ignore'))

s.sendall(b"x/2x 0x0013d0f4\n")
time.sleep(0.5)
data_ptrs = s.recv(4096)
print("=== TASK PTRS (task_list_head, current_task) ===")
print(data_ptrs.decode(errors='ignore'))

# Let's inspect the memory of the tasks
s.sendall(b"x/24x 0x01000000\n")
time.sleep(0.5)
data_heap = s.recv(4096)
print("=== HEAP START ===")
print(data_heap.decode(errors='ignore'))

s.sendall(b"quit\n")
proc.terminate()
proc.wait()
