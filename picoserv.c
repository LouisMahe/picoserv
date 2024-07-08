#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <arpa/inet.h>

struct	s_client
{
	int	fd;
	int	id;
	char	buffer[64000];
	char	stash[64000];
};

struct s_client	clients[1024];
static fd_set	recset = {0};
static fd_set	sendset = {0};
static fd_set	allset = {0};
static int	maxfd = 0;
static int	maxid = 0;
char	message[64000];

int	setSocket(int port)
{
	struct sockaddr_in	sock_add;
	int hostfd = socket(AF_INET, SOCK_STREAM, 0);
	if (hostfd < 0)
		return (-1);
	bzero(&sock_add, sizeof(sock_add));
	sock_add.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_add.sin_family = AF_INET;
	sock_add.sin_port = htons(port);

	if (bind(hostfd, (const struct sockaddr *)&sock_add, sizeof(sock_add)) == -1)
	{
		close(hostfd);
		exit(1);
	}
	return (hostfd);
}

void	addline(char* line, int sourcefd, int hostfd)
{
	int	len = 0;
	for (int fd = 0; fd <= maxfd; fd++)
	{
		if (FD_ISSET(fd, &allset) && fd != sourcefd && fd != hostfd)
		{
			len = strlen(clients[fd].buffer);
			sprintf(&clients[fd].buffer[len], "client %d: %s\n", clients[sourcefd].id, line);
		}
	}
}

void	setSets(int hostfd)
{
	maxfd = hostfd;
	bzero(clients, sizeof(clients));
	FD_SET(hostfd, &allset);
}

void	acceptClient(int hostfd)
{
	int	newfd = accept(hostfd, NULL, NULL);
	int	len;
	if (newfd < 0)
		return ;
	if (newfd > maxfd)
		maxfd = newfd;
	clients[newfd].id = maxid;
	maxid++;
	clients[newfd].buffer[0] = 0;
	FD_SET(newfd, &allset);
	for (int fd = 0; fd <= maxfd; fd++)
	{
		if (FD_ISSET(fd, &allset) && fd != hostfd && fd != newfd)
		{
			len = strlen(clients[fd].buffer);
			sprintf(&(clients[fd].buffer[len]), "server: client %d just arrived\n", clients[newfd].id);
		}
	}
}

int	exctractline(char *message, char *line)
{
	char	*needle = strstr(message, "\n");
	if (needle == NULL)
		return (0);
	*needle = 0;
	line = strcpy(line, message);
	message = strcpy(message, needle + 1);
	return (1);
}

void	acceptMessage(int recvfd, int hostfd)
{
	int	nread;
	int len;
	char	line[64000];
	message[0] = 0;
	nread = recv(recvfd, &message[0], 64000, 0);
	message[nread] = 0;
	if (nread <= 0)
	{
		printf("client %d left", clients[recvfd].id);
		for ( int fd = 0; fd <= maxfd; fd++)
		{
			if (FD_ISSET(fd, &allset) && fd != hostfd && fd != recvfd)
			{
				len = strlen(clients[fd].buffer);
				sprintf(&clients[fd].buffer[len], "server: client %d just left\n", clients[recvfd].id);
			}
		}
		FD_CLR(recvfd, &allset);
		FD_CLR(recvfd, &recset);
		FD_CLR(recvfd, &sendset);
		clients[recvfd].id = -1;
		clients[recvfd].buffer[0] = 0;
		clients[recvfd].stash[0] = 0;
		close (recvfd);
	}
	else{
		strcat(clients[recvfd].stash, message);
		while (exctractline(clients[recvfd].stash, line))
		{
			addline(line, recvfd, hostfd);
		}

	}
}

void	sendMessages(int hostfd)
{
	for (int fd = 0; fd <= maxfd; fd++)
	{
		if (FD_ISSET(fd, &sendset) && fd != hostfd)
		{
			if (send(fd, clients[fd].buffer, strlen(clients[fd].buffer), 0) < 0)
				exit(1);
			clients[fd].buffer[0] = 0;
		}
	}
}

int	main(int ac, char **av)
{
	int port = 8002;
	message[0] = 0;
	(void)av;
	if (ac != 2){
		//printf("please provide a port\n");
		//return (0);

	}
	struct sockaddr_in	sock_add;
	int hostfd = socket(AF_INET, SOCK_STREAM, 0);
	if (hostfd < 0){
		printf("socket failed\n");
		return (1);
	}
	bzero(&sock_add, sizeof(sock_add));
	sock_add.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_add.sin_family = AF_INET;
	sock_add.sin_port = htons(port);

	if (bind(hostfd, (const struct sockaddr *)&sock_add, sizeof(sock_add)) == -1)
	{
		close(hostfd);
		printf("bind fail\n");
		exit(1);
	}
	setSets(hostfd);
	listen(hostfd, 10);
	while (1)
	{
		recset = sendset = allset;
		FD_CLR(hostfd, &sendset);
		select(maxfd + 1, &recset, &sendset, NULL, NULL);
		for (int fd = 0; fd <= maxfd; fd++)
		{
			if (FD_ISSET(fd, &recset))
			{
				if (fd == hostfd)
					acceptClient(hostfd);
				else
					acceptMessage(fd, hostfd);
			}
		}
		sendMessages(hostfd);
	}
	printf("end\n");

}
