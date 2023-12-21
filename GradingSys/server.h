#pragma once
#include<cstdio>
#include<cstring>
#include<string>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<cstring>
#include<cerrno>
#include<vector>
#include<cstdlib>

#define MY_PORT 6591
#define BUF_SIZE 1024
#define MAX_QUEUE_NUM 5

struct Client
{
    int client_sock;
    int ptr = -1;
    bool islogin = false;
    bool tosend = true;
    int Cur_Dir_Addr;
    char Cur_Dir_Name[310];
    char Cur_User_Name[110];
    char Cur_Group_Name[110];
    char Cur_User_Dir_Name[310];

    char buffer[BUF_SIZE];
    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);
};

void Welcome(Client&);
void handleClient(Client&);

class Initializer {
public:
    Initializer(); // 构造函数中初始化文件系统
};

extern int Initialize();
extern bool ever_logging();