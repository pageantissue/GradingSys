#include"server.h"
#include"client.h"

void handleClient(Client& client)
{
    int client_sock = client.client_sock;

    while (1)
    {
        memset(client.buffer, 0, sizeof(client.buffer));
        int len = recv(client_sock, client.buffer, sizeof(client.buffer), 0);

        if (strcmp(client.buffer, "exit\n") == 0 || len <= 0)
            break;

        printf("Client %d send message: %s", client_sock, client.buffer);
        parseCommand(client.buffer);
        cmd(client.buffer);
        send(client_sock, client.buffer, strlen(client.buffer), 0);
        printf("Send message: %s\n", client.buffer);
    }

    close(client_sock);
}

char* parseCommand(char* buffer)
{
    return buffer;
}

int run_server()
{
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);//建立响应socket
    struct sockaddr_in server_sockaddr;//保存本地地址信息
    server_sockaddr.sin_family = AF_INET;//采用ipv4
    server_sockaddr.sin_port = htons(MY_PORT);//指定端口
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);//获取主机接收的所有响应

    if (bind(server_sock, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) == -1) {//绑定本地ip与端口
        perror("Bind Failure\n");
        printf("Error: %s\n", strerror(errno));//输出错误信息
        return -1;
    }

    printf("Listen Port : %d\n", MY_PORT);
    if (listen(server_sock, MAX_QUEUE_NUM) == -1) {//设置监听状态
        perror("Listen Error");
        return -1;
    }

    printf("Waiting for connection!\n");
    while (true)
    {
        Client client;
        client.client_sock = accept(server_sock, (struct sockaddr*)&client.client_addr, &client.length);
       if (client.client_sock == -1) {//连接失败
            perror("Connect Error");
            return -1;
        }
        printf("Connection Successful\n");
        // 创建子进程来处理客户端请求
        if (fork() == 0)
        {
            close(server_sock); // 子进程关闭服务器监听
            handleClient(client); // 处理客户端请求
            exit(0); // 子进程处理完毕后退出
        }
        else close(client.client_sock); // 父进程关闭客户端连接，继续监听
    }
   
    close(server_sock);//关闭服务器响应socket
    return 0;
}