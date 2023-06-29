#include "kcp_wrp.h"

static int number = 0;


int init(kcp_wrp *wrp) {
    // create a socket file descriptor
    wrp->sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (wrp->sockfd < 0) {
        perror("socket error!");
        exit(1);
    }
    // get server ip address and port
    bzero(&wrp->peer_addr, sizeof(struct sockaddr_in));
    wrp->peer_addr.sin_family      = PF_INET;
    wrp->peer_addr.sin_addr.s_addr = inet_addr(wrp->ipstr);
    wrp->peer_addr.sin_port        = htons(wrp->port);

    printf("client sockfd=%d, server ip=%s port=%d\n", wrp->sockfd, wrp->ipstr, wrp->port);
    return 0;
}

_Noreturn void loop(kcp_wrp *cli) {
    size_t  len = sizeof(struct sockaddr_in);
    ssize_t rcvd;
    int     ret;
    while (1) {
//        sleep_milli(1000);

        ikcp_update(cli->kcp, timestamp32());

        char buf[BUF_SIZE];
        bzero(buf, BUF_SIZE);

        // 接受数据
        rcvd = recvfrom(cli->sockfd, buf, BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *) &cli->peer_addr,
                        (socklen_t *) &len);
        if (rcvd <= 0) continue;
        printf("receive %ld data from UPD, len=%lu, content=%s\n", rcvd, strlen(buf + KCP_SEGHEAD_SZ),
               buf + KCP_SEGHEAD_SZ);

        // 如果收到kcp数据包，先交给kcp解析
        ret = ikcp_decode(cli->kcp, buf, (int) rcvd);
        if (ret < 0) {
            fprintf(stderr, "kcp decode data failed.\n");
            continue;
        }

        while (1) {
            rcvd = ikcp_recv(cli->kcp, buf, (int) rcvd);
            if (rcvd < 0) break;
        }

        printf("data interaction, ip=%s, port=%d, content=%s\n", inet_ntoa(cli->peer_addr.sin_addr),
               ntohs(cli->peer_addr.sin_port), buf);

        ret = ikcp_send(cli->kcp, cli->buf, (int) strlen(cli->buf));
        if (ret < 0) {
            fprintf(stderr, "client sent data failed.\n");
        } else {
            printf("client sent data=%s, size=%d\n", cli->buf, (int) strlen(cli->buf));
            number++;
            printf("client sent data with %d times\n", number);
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

    kcp_wrp *cli = (kcp_wrp *) malloc(sizeof(struct kcp_wrapper));
    bzero(cli, sizeof(struct kcp_wrapper));

    cli->ipstr = ipstr;
    cli->port  = atoi(port);

    init(cli);

    const char *msg = "Client:hello!";
    memcpy(cli->buf, msg, strlen(msg));

    ikcpcb *kcp = ikcp_create(0x1, (void *) cli);
    cli->kcp    = kcp;
    kcp->output = udp_output;

    ikcp_nodelay(kcp, 1, 10, 2, 1);
    ikcp_wndsize(kcp, 128, 128);

    const char *conn = "Connect";
    int        ret   = ikcp_send(cli->kcp, conn, (int) strlen(conn));
    if (ret < 0) {
        perror("ikcp_send send connection request failed.");
        return -1;
    }
    printf("ikcp_send connection success, msg=%s, ret=%d\n", conn, ret);
    loop(cli);

    return 0;
}
