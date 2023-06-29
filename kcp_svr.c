#include "kcp_wrp.h"

static int number = 0;

int init(kcp_wrp *wrp) {
    assert(wrp);
    wrp->sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (wrp->sockfd < 0) {
        perror("socket error!");
        exit(1);
    }
    bzero(&wrp->self_addr, sizeof(struct sockaddr_in));
    wrp->self_addr.sin_family      = PF_INET;
    wrp->self_addr.sin_addr.s_addr = inet_addr(wrp->ipstr);
    wrp->self_addr.sin_port        = htons(wrp->port);

    int ret = bind(wrp->sockfd, (struct sockaddr *) &wrp->self_addr, (socklen_t) sizeof(wrp->self_addr));
    if (ret < 0) {
        perror("bind error!");
        exit(1);
    }

    printf("server sockfd=%d, server bind ip=%s port=%d\n", wrp->sockfd, wrp->ipstr, wrp->port);
    return 0;
}

_Noreturn void loop(kcp_wrp *svr) {
    size_t  len = sizeof(struct sockaddr_in);
    ssize_t rcvd;
    int     ret;
    while (1) {
//        sleep_milli(1000);
        ikcp_update(svr->kcp, timestamp32());

        char buf[BUF_SIZE * 2];
        bzero(buf, BUF_SIZE * 2);

        // 接受数据
        rcvd = recvfrom(svr->sockfd, buf, BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *) &svr->peer_addr,
                        (socklen_t *) &len);
        if (rcvd <= 0) continue;
        printf("receive %ld data from UPD, len=%lu, content=%s\n", rcvd, strlen(buf + KCP_SEGHEAD_SZ),
               buf + KCP_SEGHEAD_SZ);

        // 如果收到kcp数据包，先交给kcp解析
        ret = ikcp_decode(svr->kcp, buf, (int) rcvd);
        if (ret < 0) {
            fprintf(stderr, "kcp decode data failed.\n");
            continue;
        }

        while (1) {
            // 接受真正的数据
            rcvd = ikcp_recv(svr->kcp, buf, (int) rcvd);
            if (rcvd < 0) break;
        }

        printf("data interaction, ip=%s, port=%d, content=%s\n", inet_ntoa(svr->peer_addr.sin_addr),
               ntohs(svr->peer_addr.sin_port), buf);

        ret = ikcp_send(svr->kcp, svr->buf, (int) strlen(svr->buf));
        if (ret < 0) {
            fprintf(stderr, "server sent data failed.\n");
        } else {
            printf("server sent data=%s, size=%d\n", svr->buf, (int) strlen(svr->buf));
            number++;
            printf("server sent data with %d times\n", number);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "please input ip address and port.");
        return -1;
    }
    const char *ipstr = (const char *) argv[1];
    const char *port  = (const char *) argv[2];

    kcp_wrp *svr = (kcp_wrp *) malloc(sizeof(struct kcp_wrapper));
    bzero(svr, sizeof(struct kcp_wrapper));

    svr->ipstr = ipstr;
    svr->port  = atoi(port);

    init(svr);

    const char *msg = "Server:hello!";
    memcpy(svr->buf, msg, strlen(msg));

    ikcpcb *kcp = ikcp_create(0x1, (void *) svr);
    svr->kcp    = kcp;
    kcp->output = udp_output;
    ikcp_nodelay(kcp, 1, 10, 2, 1);
    ikcp_wndsize(kcp, 128, 128);

    loop(svr);

    return 0;
}