#include"server.h"
#include"os.h"
#include"function.h"
#include<limits>
#include<unistd.h>
#include<cstdio>
#include<mutex>
using namespace std;
Client sys;

void Welcome(Client& client)
{
    int client_sock = client.client_sock;
    char buff[] = "GradingSys Greeting!\n";
    send(client_sock, buff, strlen(buff), 0);
}
int Initialize()
{
    if ((fr = fopen(GRADE_SYS_NAME, "rb")) == NULL)
    {
        fw = fopen(GRADE_SYS_NAME, "wb");
        if (fw == NULL) {
            printf("Failed to open the virtual disc file!\n");
            return 0;
        }
        fr = fopen(GRADE_SYS_NAME, "rb");
        printf("Virtual disc file openned successfully!\n");
        isLogin = false;
        strcpy(sys.Cur_User_Name, "root");
        strcpy(sys.Cur_Group_Name, "root");

        memset(Cur_Host_Name, 0, sizeof(Cur_Host_Name));
        if (gethostname(Cur_Host_Name, sizeof(Cur_Host_Name)) != 0) {
            perror("Error getting hostname");
            return 1;
        }

        Root_Dir_Addr = Inode_Start_Addr;
        sys.Cur_Dir_Addr = Root_Dir_Addr;
        strcpy(sys.Cur_Dir_Name, "/");
        printf("Formatting the file system...\n");

        if (!Format()) {
            printf("Formatting file system failed!\n");
            return 0;
        }
        printf("Formatting done.\n\n");
        //Install
        if (!Install()) {

            printf("File system installation failure!\n");
            return 0;
        }
    }
    else
    {
        fw = fopen(GRADE_SYS_NAME, "rb+");
        if (fw == NULL) {
            printf("Disk files openning failure!\n");
            return false;
        }

        isLogin = false;
        strcpy(sys.Cur_User_Name, "root");
        strcpy(sys.Cur_Group_Name, "root");

        memset(Cur_Host_Name, 0, sizeof(Cur_Host_Name));
        if (gethostname(Cur_Host_Name, sizeof(Cur_Host_Name)) != 0) {
            perror("Error getting hostname");
            return 1;
        }

        Root_Dir_Addr = Inode_Start_Addr;
        sys.Cur_Dir_Addr = Root_Dir_Addr;
        strcpy(sys.Cur_Dir_Name, "/");

        printf("Format the file system? [y/n]\n");
        char str;
        scanf("%s", &str);
        if (str == 'y') {
            if (!Format()) {
                printf("Failed to format the system!\n");
                return 0;
            }
            printf("Format done!\n");
        }


        //Install
        if (!Install()) {
            printf("File system installation failed!\n");
            return 0;
        }
        printf("File system installation done.\n");
    }
}

bool ever_logging()
{
    auto size = allClients.size();
    for (size_t i = 0; i < size; ++i)
        if (allClients[i].islogin)
            return true;
    return false;
}

void handleClient(Client& client)
{
    int client_sock = client.client_sock;
    while (1)
    {
        if (client.islogin)
        {
            char* p;
            if ((p = strstr(client.Cur_Dir_Name, client.Cur_User_Dir_Name)) == NULL)	//��ǰ�Ƿ����û�Ŀ¼��
            {
                char output_buffer[BUF_SIZE];
                snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", Cur_Host_Name, client.Cur_User_Name, client.Cur_Dir_Name);
                //[Linux@yhl /etc]
                send(client_sock, output_buffer, strlen(output_buffer), 0);
            }
            else
            {
                char output_buffer[BUF_SIZE];
                snprintf(output_buffer, BUF_SIZE, "[%s@%s %s]# ", Cur_Host_Name, client.Cur_User_Name, client.Cur_Dir_Name + strlen(client.Cur_User_Dir_Name));
                //[Linux@yhl ~/app]
                send(client_sock, output_buffer, strlen(output_buffer), 0);
            }
            memset(client.buffer, '\0', sizeof(client.buffer));
            int len = recv(client_sock, client.buffer, sizeof(client.buffer), 0);
            if (strcmp(client.buffer, "exit\n") == 0 || len <= 0)
            {
                printf("Client %d has logged out the system!\n", client_sock);
                break;
            }
            cmd(client);
        }
        else
        {
            char buff[] = "Welcome to GradingSysOS! Login first, please!\n";
            send(client_sock, buff, strlen(buff), 0);
            while (!login(client));
            strcpy(buff, "Successfully logged into our system!\n");
            send(client_sock, buff, strlen(buff), 0);
            help(client);
        }
    }
}