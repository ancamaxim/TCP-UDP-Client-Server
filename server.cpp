#include "server.h"

using namespace std;

unordered_map<string, client> database;

unordered_map<int, client> sock_id_map;

vector<int> is_active;

void close_client(pollfd *poll_fds, int i) {
	/* First 3 connections are stdin, listenTCP and sockUDP */
	if (i > 2)
		printf("Client %s disconnected.\n", sock_id_map[poll_fds[i].fd].id.c_str());

	shutdown(poll_fds[i].fd, SHUT_RDWR);
	close(poll_fds[i].fd);
	database[sock_id_map[poll_fds[i].fd].id].is_active = 0;
	sock_id_map.erase(poll_fds[i].fd);
	is_active[i] = 0;
}

void close_all_connections(pollfd *poll_fds, int num_sockets) {
    for (int i = 1; i < num_sockets; ++i)
		if (is_active[i])
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
				dp[i][j] = dp[i - 1][j] || dp[i - 1][j - 1];
	return dp[n][m];
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

void handle_subscription(TCP_subscription tcp_sub, int sockfd) {
	client& c = sock_id_map[sockfd];
	string topic = string(tcp_sub.topic);
	vector<string> topic_vector = split(topic, '/');

	if (tcp_sub.subscribe == 1) {
		/* Register subscription for corresponding client */
		c.subs[topic] = true;
	} else {
		/* Unsubscribe if topic exists */
		if (c.subs.find(topic) != c.subs.end()) {
			c.subs.erase(topic);
		}

		/* All those subscriptions which match, will be deactivated */
		for (const auto& [sub, active] : c.subs) {
			if (!active)
				continue;
			bool match = match_topic(split(sub, '/'), topic_vector) ||
						match_topic(topic_vector, split(sub, '/'));
			if (match)
				c.subs[sub] = false;
		}
	}

	database[c.id] = c;
}

void announce_clients(UDP_packet udp_pkt, sockaddr_in addr) {
	vector<string> topic = split(udp_pkt.topic, '/');

	for (const auto& [sockfd, client] : sock_id_map) {
		if (!client.is_active)
			continue;

		/* Search for subscriptions that might match */
		for (const auto& [sub, active] : client.subs) {
			if (!active)
				continue;
			bool match = match_topic(topic, split(sub, '/'));

			if (match) {
				TCP_notification tcp_notif;

				tcp_notif.ip_udp = addr.sin_addr;
				tcp_notif.port_udp = ntohs(addr.sin_port);
				memcpy(tcp_notif.topic, udp_pkt.topic, 51);
				tcp_notif.data_type = udp_pkt.data_type;

				switch (udp_pkt.data_type) {
				case INT:
					tcp_notif.payload_len = 5;
					break;
				case SHORT_REAL:
					tcp_notif.payload_len = 2;
					break;
				case FLOAT:
					tcp_notif.payload_len = 6;
					break;
				case STRING:
					tcp_notif.payload_len = 1 + strlen(udp_pkt.payload);
					break;
				default:
					tcp_notif.payload_len = 0;
					break;
				}
				/* Send TCP notification */
				int rc = send_all(sockfd, &tcp_notif, sizeof(tcp_notif));
				DIE(rc < 0, "send");

				rc = send_all(sockfd, udp_pkt.payload, tcp_notif.payload_len);
				DIE(rc < 0, "send");

				return;
			}
		}
	}
}

void parse_udp_pkt(UDP_packet *udp_pkt, char *buf) {
	memset(udp_pkt, 0, sizeof(*udp_pkt));
	memcpy(udp_pkt->topic, buf, 50);
	udp_pkt->topic[50] = '\0';
	memcpy(&(udp_pkt->data_type), buf + 50, 1);
	memcpy(udp_pkt->payload, buf + 51, 1500);
	udp_pkt->payload[1500] = '\0';
}

void run_server(int listenTCP, int sock_UDP) {
    TCP_subscription tcp_sub;
    TCP_notification tcp_notif;
    UDP_packet udp_pkt;

    char buf[sizeof(UDP_packet)];
    int num_sockets = 3, MAX_CONNECTIONS = 103, rc;
    pollfd *poll_fds;

	memset(buf, 0, sizeof(UDP_packet));
    memset(&tcp_sub, 0, sizeof(tcp_sub));
    memset(&tcp_notif, 0, sizeof(tcp_notif));
	memset(&udp_pkt, 0, sizeof(udp_pkt));

    poll_fds = (pollfd *) calloc(MAX_CONNECTIONS, sizeof(pollfd));
    DIE(poll_fds == NULL, "calloc() failed");

    rc = listen(listenTCP, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

	is_active.resize(MAX_CONNECTIONS);

    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;
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
			if (num_sockets == MAX_CONNECTIONS) {
				MAX_CONNECTIONS *= 2;

				poll_fds = (pollfd *) realloc(poll_fds, MAX_CONNECTIONS * sizeof(pollfd));
				DIE(poll_fds == NULL, "calloc() failed");

				rc = listen(listenTCP, MAX_CONNECTIONS);
				DIE(rc < 0, "listen");

				is_active.resize(MAX_CONNECTIONS);
			}
			if (poll_fds[i].fd == listenTCP) {
				/* Accept new connection */
				sockaddr_in cli_addr;
				socklen_t cli_len = sizeof(cli_addr);
				memset(&cli_addr, 0, sizeof(cli_addr));
				const int newsockfd = accept(listenTCP, 
                                (sockaddr *)&cli_addr, &cli_len);
				DIE(newsockfd < 0, "accept");

                /* Receive client ID */
                int rc = recv_all(newsockfd, buf, 11);
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
					client c = client(id, newsockfd, 1);
                    database.emplace(id, c);
					sock_id_map.emplace(newsockfd, c);
					printf("New client %s connected from %s:%d.\n", buf,
							inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

					/* Send ACK */
					buf[0] = 1;
					rc = send_all(newsockfd, buf, 1);
                	DIE(rc < 0, "send");
				} else {
					client& c = database.at(id);

					if (c.is_active == 0) {
						/* Client has been registered before, but is not active */
						c.is_active = 1;
						c.sockfd = newsockfd;
						sock_id_map.emplace(newsockfd, c);
						
						poll_fds[num_sockets].fd = newsockfd;
						poll_fds[num_sockets].events = POLLIN;
						is_active[num_sockets] = 1;
						num_sockets++;

						printf("New client %s connected from %s:%d (reconnect).\n", buf,
							inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

						/* Send ACK */
						buf[0] = 1;
						rc = send_all(newsockfd, buf, 1);
						DIE(rc < 0, "send");
					} else {
						/* Client already exists in database, send error */
						printf("Client %s already connected.\n", buf);

						memset(buf, 0, sizeof(UDP_packet));
						int rc = send_all(newsockfd, buf, 1);
						DIE(rc < 0, "send() error");

						shutdown(newsockfd, SHUT_RDWR);
						close(newsockfd);
					}
				}
            } else if (poll_fds[i].fd == sock_UDP) {
				/* Received UDP notification */
				memset(buf, 0, sizeof(udp_pkt));

				sockaddr_in from;
				socklen_t addrlen = sizeof(from);
				memset(&from, 0, sizeof(from));
				int rc = recvfrom(sock_UDP, &buf, sizeof(udp_pkt), 0,
							(sockaddr *) &from, &addrlen);
				DIE(rc < 0, "recvfrom");

				/* Process notification */
				parse_udp_pkt(&udp_pkt, buf);
				announce_clients(udp_pkt, from);
            } else if (poll_fds[i].fd == 0) {
                /* In case of exit command, close all connections + server */
                fgets(buf, sizeof(buf), stdin);

				buf[strlen(buf) - 1] = '\0';
                if (!strcmp(buf, "exit")) {
                    close_all_connections(poll_fds, num_sockets);
					free(poll_fds);
					exit(0);
				}
            } else {
				/* Received TCP message */
				int rc = recv_all(poll_fds[i].fd, &tcp_sub, sizeof(tcp_sub));
				DIE(rc < 0, "recv");

				if (rc == 0) {                    
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
	if (argc != 2) {
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