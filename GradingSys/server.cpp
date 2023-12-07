#include"server.h"
#include"os.h"
#include<limits>
#include<unistd.h>
#include<cstdio>
#include<cstdlib>
#include<iostream>

const int Superblock_Start_Addr = 0;     //44B:1block
const int InodeBitmap_Start_Addr = 1 * BLOCK_SIZE; //1024B:2block
const int BlockBitmap_Start_Addr = InodeBitmap_Start_Addr + 2 * BLOCK_SIZE;//10240B:20block
const int Inode_Start_Addr = BlockBitmap_Start_Addr + 20 * BLOCK_SIZE;//120<128: 换算成x个block
const int Block_Start_Addr = Inode_Start_Addr + INODE_NUM / (BLOCK_SIZE / INODE_SIZE) * BLOCK_SIZE;//32*16=512

const int Disk_Size = Block_Start_Addr + BLOCK_NUM * BLOCK_SIZE;
const int File_Max_Size = 10 * BLOCK_SIZE;


int Root_Dir_Addr;							//根目录inode地址
int Cur_Dir_Addr;							//当前目录:存inode地址
char Cur_Dir_Name[310];						//当前目录名
char Cur_Host_Name[110];					//当前主机名
char Cur_User_Name[110];					//当前登陆用户名
char Cur_Group_Name[110];					//当前登陆用户组名
char Cur_User_Dir_Name[310];				//当前登陆用户目录名

int nextUID;								//下一个要分配的用户标识号
int nextGID;								//下一个要分配的用户组标识号

bool isLogin;								//是否有用户登陆

FILE* fw;									//虚拟磁盘文件 写文件指针
FILE* fr;									//虚拟磁盘文件 读文件指针
SuperBlock* superblock = new SuperBlock;	//超级块指针
bool inode_bitmap[INODE_NUM];				//inode位图
bool block_bitmap[BLOCK_NUM];				//磁盘块位图

char buffer[10000000] = { 0 };				//10M，缓存整个虚拟磁盘文件
using namespace std;

int Initialize(Client& client)
{
    int client_sock = client.client_sock;
    char buff[] = "GradingSys Greeting!\n";
    send(client_sock, buff, strlen(buff), 0);
    //###############打不开文件################
    if ((fr = fopen(GRADE_SYS_NAME, "rb")) == NULL)
    {
        fw = fopen(GRADE_SYS_NAME, "wb");
        if (fw == NULL) {
            strcpy(buff, "Failed to open the virtual disc file!\n");
            send(client_sock, buff, strlen(buff), 0);
            return 0;
        }
        fr = fopen(GRADE_SYS_NAME, "rb");
        strcpy(buff, "Virtual disc file openned successfully!\n");
        send(client_sock, buff, strlen(buff), 0);
        //初始化变量
        nextUID = 0;
        nextGID = 0;
        isLogin = false;
        strcpy(Cur_User_Name, "root");
        strcpy(Cur_Group_Name, "root");

        //获取主机名
        memset(Cur_Host_Name, 0, sizeof(Cur_Host_Name));
        if (gethostname(Cur_Host_Name, sizeof(Cur_Host_Name)) != 0) {
            perror("Error getting hostname");
            return 1;
        }

        Root_Dir_Addr = Inode_Start_Addr;
        Cur_Dir_Addr = Root_Dir_Addr;
        strcpy(Cur_Dir_Name, "/");
        strcpy(buff, "Formatting the file system...\n");
        send(client_sock, buff, strlen(buff), 0);

        //系统格式化
        if (!Format()) {
            char buff1[] = "Formatting file system failed!\n";
            send(client_sock, buff1, strlen(buff1), 0);
            return 0;
        }
        strcpy(buff, "Formatting done.\n\n");
        send(client_sock, buff, strlen(buff), 0);
        //Install
        if (!Install()) {

            strcpy(buff, "File system installation failure!\n");
            send(client_sock, buff, strlen(buff), 0);
            return 0;
        }
    }
    else
    {
        fw = fopen(GRADE_SYS_NAME, "rb+"); //在原来的基础上修改文件
        if (fw == NULL) {
            char buff1[] = "Disk files openning failure!\n";
            send(client_sock, buff1, strlen(buff1), 0);
            return false;
        }
        //初始化变量
        nextUID = 0;
        nextGID = 0;
        isLogin = false;
        strcpy(Cur_User_Name, "root");
        strcpy(Cur_Group_Name, "root");

        //获取主机名
        memset(Cur_Host_Name, 0, sizeof(Cur_Host_Name));
        if (gethostname(Cur_Host_Name, sizeof(Cur_Host_Name)) != 0) {
            perror("Error getting hostname");
            return 1;
        }

        //获取根目录
        Root_Dir_Addr = Inode_Start_Addr;
        Cur_Dir_Addr = Root_Dir_Addr;
        strcpy(Cur_Dir_Name, "/");

        //是否需要格式化
        strcpy(buff, "Format the file system? [y/n]");
        send(client_sock, buff, strlen(buff), 0);
        memset(client.buffer, 0, sizeof(client.buffer)); // 初始化用户输入buffer
        recv(client_sock, client.buffer, sizeof(client.buffer), 0);
<<<<<<< Updated upstream

=======
>>>>>>> Stashed changes
        char yes[] = "y";
        if (client.buffer == yes) {
            if (!Format()) {
                char buff1[] = "Failed to format the system!\n";
                send(client_sock, buff1, strlen(buff1), 0);
                return 0;
            }
        }
        strcpy(buff, "Format done!\n");
        send(client_sock, buff, strlen(buff), 0);

        //Install
        if (!Install()) {
            char buff1[] = "File system installation failed!\n";
            send(client_sock, buff1, strlen(buff1), 0);
            return 0;
        }
        strcpy(buff, "File system installation done.\n");
        send(client_sock, buff, strlen(buff), 0);
    }
}
void handleClient(Client& client)
{
    int client_sock = client.client_sock;
    while (1)
    {
        if (isLogin)
        {
            char* p;
            if ((p = strstr(Cur_Dir_Name, Cur_User_Dir_Name)) == NULL)	//当前是否在用户目录下
            {
                char output_buffer[BUF_SIZE];
                // 使用snprintf将格式化的字符串存储到output_buffer中
                snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", Cur_Host_Name, Cur_User_Name, Cur_Dir_Name);
                //[Linux@yhl /etc]
                send(client_sock, output_buffer, strlen(output_buffer), 0);
            }
            else
            {
                char output_buffer[BUF_SIZE];
                snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", Cur_Host_Name, Cur_User_Name, Cur_Dir_Name + strlen(Cur_User_Dir_Name));
                //[Linux@yhl ~/app]
                send(client_sock, output_buffer, strlen(output_buffer), 0);
            }
            //gets(str);
            //cmd(str);
            /*useradd("felin", "123", "teacher");
            cd(Cur_Dir_Addr, "..");
            cd(Cur_Dir_Addr, "felin");
            mkdir(Cur_Dir_Addr, "ms");
<<<<<<< Updated upstream
            // ls    
=======
>>>>>>> Stashed changes
            mkfile(Cur_Dir_Addr, "tert", "helloworld");
            rmdir(Cur_Dir_Addr, "felin");
            userdel("felin");*/
            char output_buffer[BUF_SIZE];
            snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", Cur_Host_Name, Cur_User_Name, Cur_Dir_Name + strlen(Cur_User_Dir_Name));
            send(client_sock, output_buffer, strlen(output_buffer), 0);
            // 准备接收用户输入
<<<<<<< Updated upstream
            memset(client.buffer, 0, sizeof(client.buffer));
            int len = recv(client_sock, client.buffer, sizeof(client.buffer), 0);
            if (strcmp(client.buffer, "exit\n") == 0 || len <= 0)
            {
                printf("Client %d has logged out the system!\n", client_sock);
                break;
            }
=======
            memset(client.buffer, 0, sizeof(client.buffer)); // 初始化用户输入buffer
            int len = recv(client_sock, client.buffer, sizeof(client.buffer), 0);
            if (strcmp(client.buffer, "exit\n") == 0 || len <= 0)
                break;
>>>>>>> Stashed changes
            printf("Client %d send message: %s", client_sock, client.buffer);
            //parseCommand(client.buffer);
            //cmd(client.buffer);
            //send(client_sock, client.buffer, strlen(client.buffer), 0);
            //printf("Send message: %s\n", client.buffer);
        }
        else
        {
            char buff[] = "Welcome to GradingSysOS! Login first, please!\n";
            send(client_sock, buff, strlen(buff), 0);
<<<<<<< Updated upstream
            while (!login(client));
=======
            while (!login(client));	//登陆
>>>>>>> Stashed changes
            strcpy(buff, "Successfully logged into our system!\n");
            send(client_sock, buff, strlen(buff), 0);
        }
    }
    close(client_sock);
}

char* parseCommand(char* buffer)
{
    return buffer;
}

int main()
{
<<<<<<< Updated upstream
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;//ipv4
    server_sockaddr.sin_port = htons(MY_PORT);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sock, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) == -1) {//绑定本地ip与端口
        perror("Bind Failure\n");
        printf("Error: %s\n", strerror(errno));
        close(server_sock);
=======
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);//建立服务器响应socket
    struct sockaddr_in server_sockaddr;//保存本地地址信息
    server_sockaddr.sin_family = AF_INET;//采用ipv4
    server_sockaddr.sin_port = htons(MY_PORT);//指定端口
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);//获取主机接收的所有响应

    if (bind(server_sock, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) == -1) {//绑定本地ip与端口
        perror("Bind Failure\n");
        printf("Error: %s\n", strerror(errno));//输出错误信息
>>>>>>> Stashed changes
        return -1;
    }

    printf("Listen Port : %d\n", MY_PORT);
<<<<<<< Updated upstream
    if (listen(server_sock, MAX_QUEUE_NUM) == -1) {
        perror("Listen Error");
        close(server_sock);
=======
    if (listen(server_sock, MAX_QUEUE_NUM) == -1) {//设置监听状态
        perror("Listen Error");
>>>>>>> Stashed changes
        return -1;
    }

    printf("Waiting for connection!\n");
    while (true)
    {
        Client client;
        client.client_sock = accept(server_sock, (struct sockaddr*)&client.client_addr, &client.length);
        if (client.client_sock == -1) {
            perror("Connect Error");
            return -1;
        }
        printf("Connection Successful\n");
        if (fork() == 0) {
            // 子进程
            close(server_sock); // 子进程关闭服务器监听
            Initialize(client); // 重新初始化独立的Client对象
            handleClient(client); // 处理客户端请求
            close(client.client_sock); // 子进程处理完毕后关闭套接字
            exit(0); // 子进程处理完毕后退出
        }
        // 父进程继续监听，不需要额外的处理
    }

    close(server_sock);//关闭服务器响应socket
    return 0;
}