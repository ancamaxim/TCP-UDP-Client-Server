#include "server.h"

using namespace std;

unordered_map<string, client> database;

unordered_map<int, client> sock_id_map;

vector<int> is_active;

void close_client(pollfd *poll_fds, int i) {
	shutdown(poll_fds[i].fd, SHUT_RDWR);
	close(poll_fds[i].fd);
	database[sock_id_map[poll_fds[i].fd].id].is_active = 0;
	sock_id_map.erase(poll_fds[i].fd);
	is_active[i] = 0;
}

void close_all_connections(pollfd *poll_fds, int num_sockets) {
    for (int i = 1; i < num_sockets; ++i)
		close_client(poll_fds, i);
}

bool match_topic(vector<string> s, vector<string> p) {
	int n = s.size(), m = p.size();
	vector<vector<bool>> dp(n + 1, vector<bool>(m + 1, false));

	dp[0][0] = true;
	for (int i = 1; i <= n; i++)
		for (int j = 1; j <= m; j++)
			if (s[i - 1] == p[j - 1] || p[j - 1] == "+")
				dp[i][j] = dp[i - 1][j - 1];
			else if(p[j - 1] == "*")
				dp[i][j] = dp[i - 1][j];
	return dp[n][m];
}

void handle_subscription(TCP_subscription tcp_sub, int sockfd) {
	client& c = sock_id_map[sockfd];
	string topic = string(tcp_sub.topic);

	if (tcp_sub.subscribe == 1) {
		/* Register subscription for corresponding client */
		c.subs[topic] = true;
	} else {
		/* Unsubscribe if topic exists */
		if (c.subs.find(topic) != c.subs.end())
			c.subs[topic] = false;
	}
}

vector<string> split(const string& text, char delim) {
	vector<string> res;
	stringstream ss(text);
	string word;

	while(getline(ss, word, '/')) {
		res.push_back(word);
	}
	return res;
}

void announce_clients(UDP_packet udp_pkt) {
	for (const auto& [sockfd, client] : sock_id_map) {
		for (const auto& [sub, active] : client.subs) {
			if (!active)
				continue;
			bool match = match_topic(split(sub, '/'), split(udp_pkt.topic, '/'));

			if (match) {
				int rc = send_all(sockfd, &udp_pkt, sizeof(udp_pkt));
				DIE(rc < 0, "send");
			}
		}
	}
}

void run_server(int listenTCP, int sock_UDP) {
    TCP_subscription tcp_sub;
    TCP_notification tcp_notif;
    UDP_packet udp_pkt;

    char buf[MAX_TCP_MESSAGE];
    int num_sockets = 3, MAX_CONNECTIONS = 33, rc;
    pollfd *poll_fds;

	memset(buf, 0, MAX_TCP_MESSAGE);
    memset(&tcp_sub, 0, sizeof(tcp_sub));
    memset(&tcp_notif, 0, sizeof(tcp_notif));

    poll_fds = (pollfd *) calloc(MAX_CONNECTIONS, sizeof(pollfd));
    DIE(poll_fds == NULL, "calloc() failed");

    rc = listen(listenTCP, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

	is_active.resize(MAX_CONNECTIONS);

    poll_fds[0].fd = 0;
    poll_fds[0].fd = POLLIN;
    poll_fds[1].fd = sock_UDP;
    poll_fds[1].events = POLLIN;
    poll_fds[2].fd = listenTCP;
    poll_fds[2].events = POLLIN;

	is_active[0] = is_active[1] = is_active[2] = 1;

    while (1) {
		rc = poll(poll_fds, num_sockets, -1);
		DIE(rc < 0, "poll");

		for (int i = 0; i < num_sockets; i++) {
			if (!(poll_fds[i].revents & POLLIN) || !is_active[i])
				continue;
			if (poll_fds[i].fd == listenTCP) {
				/* Accept new connection */
				struct sockaddr_in cli_addr;
				socklen_t cli_len = sizeof(cli_addr);
				const int newsockfd = accept(listenTCP, 
                                (struct sockaddr *)&cli_addr, &cli_len);
				DIE(newsockfd < 0, "accept");

                /* Receive client ID */
                int rc = recv_all(poll_fds[i].fd, buf, 11);
                DIE(rc < 0, "recv");

				/* First ever time of registration */
				string id = string(buf);
                if (database.find(id) == database.end()) {
					/* Add socket to poll */
					poll_fds[num_sockets].fd = newsockfd;
					poll_fds[num_sockets].events = POLLIN;
					is_active[num_sockets] = 1;
					num_sockets++;

					/* Register client into database */
                    database[id] = sock_id_map[newsockfd] = client(id, newsockfd, 1);
					printf("New client %s connected from %s:%d.\n", buf,
							inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
				} else {
					client& c = database[id];

					if (!c.is_active) {
						/* Client has been registered before, but is not active */
						c.is_active = 1;
						sock_id_map.erase(c.sockfd);
						c.sockfd = newsockfd;
						sock_id_map[newsockfd] = c;
					} else {
						/* Client already exists in database, send error */
						printf("Client %s already connected.\n", buf);

						memset(buf, 0, MAX_TCP_MESSAGE);
						int rc = send_all(newsockfd, buf, sizeof(tcp_notif));
						DIE(rc < 0, "send() error");

						shutdown(newsockfd, SHUT_RDWR);
						close(newsockfd);
					}
				}
            } else if (poll_fds[i].fd == sock_UDP) {
				/* Received UDP notification */
				sockaddr from;
				socklen_t addrlen;
				int rc = recvfrom(sock_UDP, &udp_pkt, sizeof(udp_pkt), 0,
							(struct sockaddr *) &from, &addrlen);
				DIE(rc < 0, "recvfrom");

				/* Process notification */
				announce_clients(udp_pkt);
            } else if (poll_fds[i].fd == 0) {
                /* In case of exit command, close all connections + server */
                fgets(buf, sizeof(buf), stdin);

                if (!strcmp(buf, "exit\n")) {
                    close_all_connections(poll_fds, num_sockets);
					free(poll_fds);
					exit(0);
				}
            } else {
				/* Received TCP message */
				int rc = recv_all(poll_fds[i].fd, &tcp_sub, sizeof(tcp_sub));
				DIE(rc < 0, "recv");

				if (rc == 0) {
					printf("Client %s disconnected.\n", sock_id_map[poll_fds[i].fd].id.c_str());
                    
                    /* Delete socket */
					close_client(poll_fds, i);
				} else {
					/* Valid TCP message: subscribe / unsubscribe */
					handle_subscription(tcp_sub, poll_fds[i].fd);
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
	if (setsockopt(listenTCP, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
	}
	
    if (setsockopt(sock_UDP, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
	}

	/* Disable Nagel's algorithm */
	if (setsockopt(listenTCP, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
		perror("setsockopt(TCP_NODELAY) failed");
	}

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
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