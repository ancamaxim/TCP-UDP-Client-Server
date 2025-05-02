#include "subscriber.h"

using namespace std;

void parse_tcp(TCP_subscription *tcp_pkt, char *buf) {
    char *sub = strtok(buf, " ");
    if (!sub)
        exit(-1);
    char *topic = strtok(NULL, " ");
    if (!topic)
        exit(-1);

    strcpy(tcp_pkt->topic, topic);
    if (!strcmp(sub, "subscribe"))
        tcp_pkt->subscribe = 1;
    else if (!strcmp(sub, "unsubscribe"))
        tcp_pkt->subscribe = 0;
}

void print_notification(TCP_notification tcp_notif, char *ip_udp, char *payload) {

    printf("%s:%d - %s - %s - ", ip_udp, tcp_notif.port_udp,
            tcp_notif.topic, data_type_to_string(tcp_notif.data_type));

    if (tcp_notif.data_type == INT) {
        int nr = 0;
        unsigned char sign = *(unsigned char *)(payload);

        nr = ntohl(*(uint32_t *)(payload + 1));

        if (sign == 1)
            nr = ~nr + 1;

        printf("%d\n", nr);
    } else if (tcp_notif.data_type == SHORT_REAL) {
        uint16_t nr = ntohs(*(uint16_t *)payload);

        printf("%.*f\n", 2, nr * 1.0 / 100);

    } else if (tcp_notif.data_type == FLOAT) {
        int nr = 0;
        char sign = *(unsigned char *)(payload);
        uint8_t exp = *(unsigned char *)(payload + 5);
        long long p = 1;

        nr = ntohl(*(uint32_t *)(payload + 1));
        p = pow(10, exp);

        if (sign == 1)
            nr = ~nr + 1;

        printf("%.*f\n", exp, nr * 1.0 / p);

    } else if (tcp_notif.data_type == STRING) {
        printf("%.1500s\n", payload);
    }
}

void run_client(int sockfd)
{
	int rc;
	char buf[sizeof(UDP_packet)];
	struct pollfd poll_fds[2];
    TCP_subscription tcp_sub;
    TCP_notification tcp_notif;

	memset(buf, 0, sizeof(UDP_packet));
    memset(&tcp_sub, 0, sizeof(tcp_sub));
    memset(&tcp_notif, 0, sizeof(tcp_notif));

	poll_fds[0].fd = 0;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = sockfd;
	poll_fds[1].events = POLLIN;

	while (1) {
		rc = poll(poll_fds, 2, -1);
		DIE(rc < 0, "poll failed.");
		
        /* Subscribe / Unsubscribe message */
		if (poll_fds[0].revents & POLLIN) {
			fgets(buf, 70, stdin);
            buf[strlen(buf) - 1] = '\0';

            if (!strcmp(buf, "exit")) {
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                exit(0);
            }
            parse_tcp(&tcp_sub, buf);

			rc = send_all(sockfd, &tcp_sub, sizeof(tcp_sub));
            DIE(rc < 0, "send()");

            if (tcp_sub.subscribe)
                printf("Subscribed to topic %s.\n", tcp_sub.topic);
            else
                printf("Unsubscribed from topic %s.\n", tcp_sub.topic);

		} else if (poll_fds[1].revents & POLLIN) {
			/* Receive notification from server */
			int rc = recv_all(sockfd, &tcp_notif, sizeof(tcp_notif));
            DIE(rc < 0, "recv()");

            char *ip_udp, *payload;

            payload = (char *) calloc(tcp_notif.payload_len, 1);
            rc = recv_all(sockfd, payload, tcp_notif.payload_len);
            DIE(rc < 0, "recv()");
                                  
            /* Server closed */
            if (rc <= 0) {
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                exit(0);
            }

            ip_udp = strdup(inet_ntoa(tcp_notif.ip_udp));
            print_notification(tcp_notif, ip_udp, payload);
            free(ip_udp);
		}
	}

}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("\n Usage: %s <id_client> <ip_server> <port_server>\n", argv[0]);
		return 1;
	}
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	uint16_t port;
	int rc = sscanf(argv[3], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");
    DIE(strlen(argv[1]) > 10, "Given ID is invalid.\n");

	/* TCP send socket */
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");
    
    /* Make address reusable */
	const int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
	}
	
	/* Disable Nagel's algorithm */
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
	}
    
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
	DIE(rc <= 0, "inet_pton");

	rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "connect");

    char buf[11];

    /* Send client id first */
    memset(buf, 0, 11);
    strcpy(buf, argv[1]);
    rc = send_all(sockfd, buf, 11);
    DIE(rc < 0, "send client_id failed");

    /* Receive ACK */
    rc = recv_all(sockfd, buf, 1);
    DIE(rc < 0, "recv()");

    if (buf[0])
	    run_client(sockfd);

	close(sockfd);

	return 0;
}
