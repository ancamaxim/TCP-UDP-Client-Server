#include "utils.h"

int recv_all(int sockfd, void *buffer, size_t len)
{
	size_t bytes_received = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;

	while (bytes_remaining > 0)
	{
		int rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
		DIE(rc < 0, "recv");

		if (rc == 0)
			break;
		bytes_received += rc;
		bytes_remaining -= rc;
	}
	return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len)
{
	size_t bytes_sent = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;

	while (bytes_remaining > 0) {
		int rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
		DIE(rc < 0, "send");

		bytes_sent += rc;
		bytes_remaining -= rc;
	}

	return bytes_sent;
}

const char *data_type_to_string(DATA_TYPE type)
{
	switch (type) {
	case INT:
		return "INT";
	case SHORT_REAL:
		return "SHORT_REAL";
	case FLOAT:
		return "FLOAT";
	case STRING:
		return "STRING";
	default:
		return "";
	}
}
