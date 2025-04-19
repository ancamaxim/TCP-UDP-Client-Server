#include "subscriber.h"

using namespace std;


void parse_tcp(TCP_subscription *tcp_pkt, char *buf) { 
    char *sub = strtok(buf, " ");
    char *topic = strtok(NULL, " ");

    strcpy(tcp_pkt->topic, topic);
    if (!strcmp(sub, "subscribe")) {
        tcp_pkt->subscribe = 1;
        printf("Subscribed to topic %s.\n", topic);
    } else {
        tcp_pkt->subscribe = 0;
        printf("Unsubscribed to topic %s.\n", topic);
    }
}

void print_notification(TCP_notification tcp_notif) {

    char *payload = tcp_notif.pkt.payload;

    printf("%s:%d - %s - %s - ", tcp_notif.ip_udp, tcp_notif.port_udp,
            tcp_notif.pkt.topic, dataTypeToString(tcp_notif.pkt.data_type));

    if (tcp_notif.pkt.data_type == INT) {
        int nr = 0;
        char sign = *(tcp_notif.pkt.payload);

        nr = htonl((uint32_t)*(tcp_notif.pkt.payload + 1));

        if (sign == 1)
            nr = ~nr + 1;

        printf("%d\n", nr);
    } else if (tcp_notif.pkt.data_type == SHORT_REAL) {
        uint16_t nr = htons(*(uint16_t *) tcp_notif.pkt.payload);

        printf("%f\n", nr * 1.0 / 100);
    } else if (tcp_notif.pkt.data_type == FLOAT) {
        int nr = 0;
        char sign = *(tcp_notif.pkt.payload);
        uint8_t exp = *(tcp_notif.pkt.payload + 5);
        int p = 1;

        nr = htonl((uint32_t)*(tcp_notif.pkt.payload + 1));
        p = pow(10, exp);

        if (sign == 1)
            nr = ~nr + 1;

        printf("%f\n", nr * 1.0 / p);
    } else if (tcp_notif.pkt.data_type == STRING) {
        printf("%s\n", tcp_notif.pkt.payload);
    }
}

void run_client(int sockfd)
{
	int rc;
	char buf[MAX_TCP_MESSAGE];
	struct pollfd poll_fds[2];
    TCP_subscription tcp_sub;
    TCP_notification tcp_notif;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	memset(buf, 0, MAX_TCP_MESSAGE);
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
			fgets(buf, sizeof(buf), stdin);
            buf[strlen(buf) - 1] = '\0';
			
            if (!strcmp(buf, "exit")) {
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                exit(0);
            }
            parse_tcp(&tcp_sub, buf);
			send_all(sockfd, &tcp_sub, sizeof(tcp_sub));
		} else if (poll_fds[1].revents & POLLIN) {
			/* Receive notification from server */
			int rc = recv_all(sockfd, &tcp_notif, sizeof(tcp_notif));
			if (rc <= 0)
				break;
            
            /* Invalid client id, will close client */
            if (tcp_notif.ip_udp[0] == '\0') {
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                exit(0);
            }
            print_notification(tcp_notif);
		}
	}

}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("\n Usage: %s <id_client> <ip_server> <port_server>\n", argv[0]);
		return 1;
	}

	uint16_t port;
	int rc = sscanf(argv[3], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");
    DIE(strlen(argv[1]) > 10, "Given ID is invalid.\n");

	/* TCP send socket */
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
	DIE(rc <= 0, "inet_pton");

	rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "connect");

    rc = send_all(sockfd, argv[1], 11);
    DIE(rc < 0, "send client_id failed");

	run_client(sockfd);

	close(sockfd);

	return 0;
}
