#pragma once
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<cerrno>
#include<cstdlib>

#define MY_PORT 6591//端口号
#define BUF_SIZE 1024//最大缓存
#define MAX_QUEUE_NUM 5//最大连接数

struct Client //服务端的客户
{
    int client_sock;
    char buffer[BUF_SIZE]; //缓存用户的输入
    struct sockaddr_in client_addr;//保存客户端地址信息
    socklen_t length = sizeof(client_addr);//需要的内存大小
};

char* parseCommand(char* message); // 解析用户的输入，并返回命令行语言