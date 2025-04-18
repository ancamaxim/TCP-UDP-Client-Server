#include "server.h"

using namespace std;

unordered_map<string, vector<string>> database;

void close_all_connections(pollfd *poll_fds, int num_sockets) {
    for (int i = 1; i < num_sockets; ++i)
        close(poll_fds[i].fd);
    exit(0);
}

void delete_client(pollfd *poll_fds, int i, int *num_sockets) {
    for (int j = i; j < *num_sockets - 1; j++)
        poll_fds[j] = poll_fds[j + 1];
    --(*num_sockets);
    // database.erase()
}

void run_server(int listenTCP, int sock_UDP) {
    TCP_packet tcp_pkt;
    UDP_packet udp_pkt;

    char buf[MAX_TCP_MESSAGE];
    int num_sockets = 3, MAX_CONNECTIONS = 33, rc;
    pollfd *poll_fds;

    poll_fds = (pollfd *) calloc(MAX_CONNECTIONS, sizeof(pollfd));
    DIE(poll_fds == NULL, "calloc() failed");

    rc = listen(listenTCP, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    poll_fds[0].fd = 0;
    poll_fds[0].fd = POLLIN;
    poll_fds[1].fd = sock_UDP;
    poll_fds[1].events = POLLIN;
    poll_fds[2].fd = listenTCP;
    poll_fds[2].events = POLLIN;

    while (1) {
		rc = poll(poll_fds, num_sockets, -1);
		DIE(rc < 0, "poll");

		for (int i = 0; i < num_sockets; i++)
		{
			if (!(poll_fds[i].revents & POLLIN))
				continue;
			if (poll_fds[i].fd == listenTCP) {
				/* Accept new connection */
				struct sockaddr_in cli_addr;
				socklen_t cli_len = sizeof(cli_addr);
				const int newsockfd = accept(listenTCP, 
                                (struct sockaddr *)&cli_addr, &cli_len);
				DIE(newsockfd < 0, "accept");

                /* Add socket to poll */
				poll_fds[num_sockets].fd = newsockfd;
				poll_fds[num_sockets].events = POLLIN;
				num_sockets++;

                /* Receive client ID */
                int rc = recv_all(poll_fds[i].fd, &buf, 10);
                DIE(rc < 0, "recv");

                /* Register client into database */
                if (database.find(string(buf)) == database.end()) {
                    database[string(buf)] = vector<string>{};
                } else {
                    /* Client already exists in database, send error */
                }

				printf("New client %s, connected from \n",
					   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
					   newsockfd);
            } else if (poll_fds[i].fd == sock_UDP) {
                // info about 
            } else if (poll_fds[i].fd == 0) {
                // for exit command
                fgets(buf, sizeof(buf), stdin);
                if (!strcmp(buf, "exit\n"))
                    close_all_connections(poll_fds, num_sockets);
            } else {
				int rc = recv_all(poll_fds[i].fd, &received_packet,
								  sizeof(received_packet));
				DIE(rc < 0, "recv");
				if (rc == 0)
				{
					printf("Socket-ul client %d a inchis conexiunea\n", i);
					close(poll_fds[i].fd);
                    
                    /* Delete socket */
					delete_client()
				} else {
					printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n",
						   poll_fds[i].fd, received_packet.message);
					/* Send message to clients */
					for (int j = 0; j < num_sockets; ++j)
						if (poll_fds[j].fd != listenTCP && i != j)
							send_all(poll_fds[j].fd, &received_packet, sizeof(received_packet));
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("\n Usage: %s <port>\n", argv[0]);
		return 1;
	}
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    /* Convert input port to integer */
	uint16_t port;
	int rc = sscanf(argv[1], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

    /* Sockets for connection listening */
	const int listenTCP = socket(AF_INET, SOCK_STREAM, 0);
	DIE(listenTCP < 0, "socket");

    const int sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sock_UDP < 0, "socket");

	sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(sockaddr_in);

	/* Make address reusable */
	const int enable = 1;
	if (setsockopt(listenTCP, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");
    if (setsockopt(sock_UDP, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	rc = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
	DIE(rc <= 0, "inet_pton");

	rc = bind(listenTCP, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind");
    rc = bind(sock_UDP, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind");

    run_server(listenTCP, sock_UDP);

	close(listenTCP);
    close(sock_UDP);

	return 0;
}