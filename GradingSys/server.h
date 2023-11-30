#pragma once
#include"client.h"
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<cerrno>
#include<cstdlib>

#define MY_PORT 1234//端口号
#define BUF_SIZE 1024//最大缓存
#define MAX_QUEUE_NUM 5//最大连接数

char* parseCommand(char* message); // 解析用户的输入，并返回命令行语言

char* cmd(char* message); // 命令行执行程序，返回执行结果，
// 输入的message为parseCommand函数处理过后的字符串

int run_server();