#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(void)
{ 
	struct ip_mreq multiaddress; 	
	int serveraddress_len;	
	int client_socket;
	struct sockaddr_in serveraddress;

	/*服务端的地址，
	 *
	 * 2.组播：  
*  组播需要用到特定的地址：D类地址（224.0.0.0~239.255.255.255）  
*  发送消息的一方需要指定一个组播地址（程序中为224.0.0.1），接受消息的一方需要通过setsockopt()函数中IP_ADD_MEMBERSHIP选项加入组播组，结束
通信时先通过setsockopt()函数中IP_DROP_MEMBERSHIP选项退出组播组，再调用close()函数关闭socket
*/
	memset(&serveraddress, 0, sizeof(serveraddress));
	serveraddress.sin_family = AF_INET;
	serveraddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddress.sin_port = htons(5555);
	if((client_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) 
	{ 
		perror("socket");
		return 0;
	}
	
	int opt = 1;
	if(setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR,&opt, sizeof(opt)) < 0)
	{ 
		perror("setsockopt:SO_REUSEADDR");
		return 0;
	}
	/*客户端的socket绑定服务地址*/
	if(bind(client_socket, (struct sockaddr *)&serveraddress, sizeof(serveraddress)) < 0)
	{
		perror("bind"); 
		return 0;
    }

    /*组播组地址*/
	// 在同一个主机上进行广播设置套接口
	multiaddress.imr_multiaddr.s_addr = inet_addr("224.0.0.1"); 
	multiaddress.imr_interface.s_addr = htonl(INADDR_ANY);
	
	// 加入广播组
	if (setsockopt(client_socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&multiaddress, sizeof(multiaddress)) < 0)
	{   
		perror("IP_ADD_MEMBERSHIP");
		return 0;
    } 

	while(1) 
	{    
		char buf[200];
		serveraddress_len = sizeof(serveraddress);
		/*客户端的socket 从服务端的地址接受udp*/
		if(recvfrom(client_socket, buf, 200, 0,	(struct sockaddr *)&serveraddress,(socklen_t*) &serveraddress_len) < 0) 
		{ 
			perror("recvfrom");   
		}  
		printf("msg from server: %s\n", buf);

		if(strcmp(buf,"quit") == 0)
		{			
			if(setsockopt(client_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,&multiaddress, sizeof(multiaddress)) < 0) 
			{ 
				perror("setsockopt:IP_DROP_MEMBERSHIP"); 
			}   
			close(client_socket); 
			return 0;
		}
	} 	
}
