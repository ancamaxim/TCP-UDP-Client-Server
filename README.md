# README

Client-server application for TCP and UDP message sending and handling implemented in C++.

This application consists of 2 main files:

* __server.cpp__:

    The server interacts efficiently with clients and the user using BSD Sockets and `poll()`.
    It's very important to note the usage of structures for message handling in the application layer.
    The motivation for this approach is better maintainability and readibility, since working with
    simple buffers would be difficult to scale further and prone to memory errors. The structures used are defined in utils.cpp.

    The server handles connections with both TCP and UDP clients and with the user (through stdin). Both the connections array and the listening queue are reallocated when needed, thus being capable of supporting a large number of clients.

    The client struct holds the necessary information for each client: id, file descriptor for its connection with the server, `is_active` status and a map with subscriptions and their status ((in)active).

    The server keeps track of clients by registering them into a database - hashmap which maps clients' id to their information for faster access, and also into
    a map which associates clients' socket file descriptor to their information.
    Another key data structure is a bool vector, `is_active`, which keeps track whether
    the client is still connected.

    Once a client has connected, server sends an ACK / NACK, confirming whether the client's id was correct.

    When the server receives a new UDP notification, it parses it into the according
    structure for sending (UDP_packet - contains information about the packet: topic + data_type + payload) and sends it to every client which is a subscribed to a topic that matches the notification's topic.

    For efficient string matching I used a dynamic programming approach (function `match_topic()`) and for sending the message to clients, I split it into 2 separate
    parts: notification information (such as: IP and port of UDP client, topic,
    data type and payload length) + payload. This approach allows the server to send
    variable sized payload, instead of fixed size (1500 bytes), thus making transmission optimal.

    The server handles subscriptions by keeping a map of subscriptions for each client: the map is used for marking the subscription as active / inactive.

    In case of `exit` command, the server closes all active connections and exits.

* __subscriber.cpp__:

    The client communicates with the server using TCP and can (un)subscribe to topics, receiving notifications published via UDP.

    The clients sends its `client_id` immediately after connection and waits for a 1-byte ACK from the server before entering the main loop. 
    The main interaction logic is implemented in the `run_client()` function, which uses `poll()` to monitor: stdin (for user commands) and the TCP socket (for server messages).

    The user's command from stdin is parsed using the `parse_tcp()` function, which fills a `TCP_subscription` structure with the appropriate data and sends it to the server, making sure to differentiate between the command type: (un)subscribe.
    
    The notification received from the server is printed, using `print_notification()`, which outputs the received message according to a certain format. This function is also responsible for doing the dirty work of payload parsing, based on data type. 

__Note__: I also made sure to disable Nagle's algorithm for low-latency messaging.
