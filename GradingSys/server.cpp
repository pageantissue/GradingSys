#include"server.h"
#include"os.h"
#include<limits>
#include<unistd.h>
#include<cstdio>
#include<cstdlib>
#include<iostream>
using namespace std;
extern int count;

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
    // 全局变量局部化
    client.Root_Dir_Addr = Root_Dir_Addr;
    client.Cur_Dir_Addr = Cur_Dir_Addr;
    strcpy(client.Cur_Dir_Name, Cur_Dir_Name);
    strcpy(client.Cur_Group_Name, Cur_Group_Name);
    strcpy(client.Cur_Host_Name, Cur_Host_Name);
    strcpy(client.Cur_User_Dir_Name, Cur_User_Dir_Name);
    strcpy(client.Cur_User_Name, Cur_User_Name);
}
void handleClient(Client& client)
{
    int client_sock = client.client_sock;
    while (1)
    {
        if (isLogin)
        {
            char* p;
            count++;
            if ((p = strstr(client.Cur_Dir_Name, client.Cur_User_Dir_Name)) == NULL)	//当前是否在用户目录下
            {
                char output_buffer[BUF_SIZE];
                // 使用snprintf将格式化的字符串存储到output_buffer中
                snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", client.Cur_Host_Name, client.Cur_User_Name, client.Cur_Dir_Name);
                //[Linux@yhl /etc]
                send(client_sock, output_buffer, strlen(output_buffer), 0);
            }
            else
            {
                char output_buffer[BUF_SIZE];
                snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", client.Cur_Host_Name, client.Cur_User_Name, client.Cur_Dir_Name + strlen(client.Cur_User_Dir_Name));
                //[Linux@yhl ~/app]
                send(client_sock, output_buffer, strlen(output_buffer), 0);
            }
            //gets(str);
            //cmd(str);
            /*useradd("felin", "123", "teacher");
            cd(Cur_Dir_Addr, "..");
            cd(Cur_Dir_Addr, "felin");
            mkdir(Cur_Dir_Addr, "ms");
            mkfile(Cur_Dir_Addr, "tert", "helloworld");
            rmdir(Cur_Dir_Addr, "felin");
            userdel("felin");*/
            // 准备接收用户输入
            memset(client.buffer, 0, sizeof(client.buffer));
            int len = recv(client_sock, client.buffer, sizeof(client.buffer), 0);
            if (strcmp(client.buffer, "exit\n") == 0 || len <= 0)
            {
                printf("Client %d has logged out the system!\n", client_sock);
                break;
            }
            cmd(client, count);
        }
        else
        {
            char buff[] = "Welcome to GradingSysOS! Login first, please!\n";
            send(client_sock, buff, strlen(buff), 0);
            while (!login(client));	//登陆
            strcpy(buff, "Successfully logged into our system!\n");
            send(client_sock, buff, strlen(buff), 0);
            system("cls");
            fclose(fw);		//释放文件指针
            fclose(fr);		//释放文件指针
        }
    }
    close(client_sock);
}