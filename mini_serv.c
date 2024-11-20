#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_CLIENT 1024
#define BUFFER_SIZE 500000

int maxSocket;
char buf[BUFFER_SIZE];
char outbuf[BUFFER_SIZE];
char *clientBuffer[MAX_CLIENT];
int clientSocket[MAX_CLIENT];
int next_id = 0;
fd_set readyRead, readyWrite, activeSocket;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void sendAll(int except)
{
	for(int i = 0; i <= maxSocket; i ++)
		if(i != except && FD_ISSET(i, &readyWrite))
			send(i, outbuf, strlen(outbuf), 0);
}

void err(char *str)
{
	write(2, str, strlen(str));
	write(2, "\n", 1);
	exit(1);
}

int main(int argc, char **argv)
{
	int sockfd, connfd;
	struct sockaddr_in servaddr;

	if (argc != 2)
		err("Wrong number of arguments");

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		err("Fatal error");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		err("Fatal error");

	if (listen(sockfd, MAX_CLIENT) != 0)
		err("Fatal error");

	FD_ZERO(&activeSocket);
	FD_SET(sockfd, &activeSocket);
	maxSocket = sockfd;
	memset(clientBuffer, 0, sizeof(clientBuffer));

	while(1)
	{
		readyRead = readyWrite = activeSocket;
		select(maxSocket + 1, &readyRead, &readyWrite, NULL, NULL);

		for (int sock = 0; sock <= maxSocket; sock ++)
		{
			if (FD_ISSET(sock, &readyRead))
			{
				if(sock == sockfd)
				{
					connfd = accept(sockfd, NULL, NULL);
					if (connfd > maxSocket)
						connfd = maxSocket;
					clientSocket[connfd] = next_id++;
					FD_SET(connfd, &activeSocket);
					sprintf(outbuf, "server: client %d just arrived\n", next_id -1);
					sendAll(connfd);
				}
				else
				{
					memset(buf, 0, sizeof(buf));
					int nbyte = recv(sock, buf, 0, sizeof(buf));
					if(nbyte <= 0)
					{
						free(clientBuffer[sock]);
						clientBuffer[sock] = NULL;
						FD_CLR(sock, &activeSocket);
						sprintf(outbuf, "server: client %d just left\n", clientSocket[sock]);
						sendAll(sock);
						close(sock);
					}
					else
					{
						clientBuffer[sock] = str_join(clientBuffer[sock], buf);
						char *msg;
						while(extract_message(&clientBuffer[sock], &msg) == 1)
						{
							sprintf(outbuf, "client %d: %s", clientSocket[sock], msg);
							sendAll(sock);
							free(msg);
						}
					}
				}
			}
		}
	}
}
