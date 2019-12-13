#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#define Sock_file   "/tmp/initclient-sock"

int main() 
{
	int len = 0;
    int sockfd = -1;
	int retbytes = 0;
	struct sockaddr_un address = {0};

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
        printf("socket failed");
		return -1;
	}

    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, Sock_file);
	len = sizeof(address);

	// 向服务器发送连接请求
	if (connect(sockfd, (struct sockaddr*)&address, len) == -1)
	{
        perror("connect failed");
		close(sockfd);
		return -1;
	}

	char msg[256] = { 0 };
	strcpy(msg, "<skinit><app>1</app><tick><tp>1</tp></tick></skinit>");
	len = (int)strlen(msg);
	retbytes = write(sockfd, msg, len);
	if (retbytes == -1)
	{
		perror("socket send data failed");
		close (sockfd);
		return -1;
	}
	
	close (sockfd);
	return 0;
}

// 编译指令
// g++ client.cpp -o client