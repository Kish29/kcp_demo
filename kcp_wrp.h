#include "ikcp.h"
#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "sys/socket.h"
#include "assert.h"
#include "sys/time.h"
#include "time.h"

/**
 * kcp segment message 24B
 * |---------------|--------|--------|-----------|
 * |    4B conv    | 1B cmd | 1B frg |   2B wnd  |
 * |---------------|--------|--------|-----------|
 * |         4B ts          |        4B sn       |
 * |------------------------|--------------------|
 * |         4B una         |        4B len      |
 * |------------------------|--------------------|
 * |                      data                   |
 * |------------------------|--------------------|
 * */

// define buf_size 512B
const size_t BUF_SIZE       = 1 << 9;
const size_t KCP_SEGHEAD_SZ = 24;

struct kcp_wrapper {
    int                sockfd;
    const char         *ipstr;
    int16_t            port;
    ikcpcb             *kcp;
    struct sockaddr_in peer_addr;
    struct sockaddr_in self_addr;
    char               buf[BUF_SIZE];
};

typedef struct kcp_wrapper kcp_wrp;

static INLINE uint64_t timestamp64() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) (tv.tv_sec * (uint64_t) 1000 + (tv.tv_usec / (uint64_t) 1000));
}

static INLINE uint32_t timestamp32() {
    return (uint32_t) (timestamp64() & 0xfffffffful);
}

static INLINE void sleep_milli(size_t millisec) {
    struct timespec ts;
    ts.tv_sec  = (time_t) (millisec / 1000);
    ts.tv_nsec = (long) (millisec % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    kcp_wrp *wrp = (kcp_wrp *) user;
    assert(wrp);
    ssize_t snd = sendto(wrp->sockfd, buf, (size_t) len, 0, (const struct sockaddr *) &wrp->peer_addr,
                         sizeof(struct sockaddr_in));
    if (snd < 0) {
        perror("UDP send data failed.");
        return -1;
    }
    printf("UDP_OUTPUT send=%lu bytes, content=%s\n", snd - KCP_SEGHEAD_SZ, buf + KCP_SEGHEAD_SZ);
    return (int) snd;
}

