#include<cstdio>
#include<cstdlib>
#include<sys/wait.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<thread>
#include<vector>
#include<mutex>
#include<condition_variable>
#include<sys/mman.h>
#include"server.h"
#include"os.h"

const int Superblock_Start_Addr = 0;     //44B:1block
const int InodeBitmap_Start_Addr = 1 * BLOCK_SIZE; //1024B:2block
const int BlockBitmap_Start_Addr = InodeBitmap_Start_Addr + 2 * BLOCK_SIZE;//10240B:20block
const int Inode_Start_Addr = BlockBitmap_Start_Addr + 20 * BLOCK_SIZE;//120<128: 换算成x个block
const int Block_Start_Addr = Inode_Start_Addr + INODE_NUM / (BLOCK_SIZE / INODE_SIZE) * BLOCK_SIZE;//32*16=512  //num 1024 * size 128 / block_size 512 = x block
const int Modified_inodeBitmap_Start_Addr = Block_Start_Addr + BLOCK_NUM * BLOCK_SIZE;      //用于增量转储的inode位图
const int FCacheBitmap_Start_Addr = Modified_inodeBitmap_Start_Addr + 2 * BLOCK_SIZE;//用于存储一级缓存块的block bitmap
const int FCache_Start_Addr = FCacheBitmap_Start_Addr + 2 * BLOCK_SIZE;//1024B:20B 一级缓存block数量为1024个

const int Backup_Start_Addr = 0;
const int Backup_Block_Start_Addr = Backup_Start_Addr + INODE_NUM;

const int Disk_Size = Block_Start_Addr + (BLOCK_NUM + 2) * BLOCK_SIZE + (2 + FCACHE_NUM) * BLOCK_SIZE;//增加板块
const int File_Max_Size = 10 * BLOCK_SIZE;

const int Start_Addr = 0;

int Root_Dir_Addr;							//根目录inode地址
char Cur_Host_Name[110];					//当前主机名

int nextUID = 0;								//下一个要分配的用户标识号
int nextGID = 0;								//下一个要分配的用户组标识号

bool isLogin = false; 								//是否有用户登陆

FILE* fw = nullptr;									//虚拟磁盘文件 写文件指针
FILE* fr = nullptr;									//虚拟磁盘文件 读文件指针
SuperBlock* superblock = new SuperBlock;	//超级块指针
bool inode_bitmap[INODE_NUM];				//inode位图
bool block_bitmap[BLOCK_NUM];				//磁盘块位图
bool modified_inode_bitmap[INODE_NUM];      //增量转储 0:未被修改；1:已修改
FILE* bfw = nullptr;
FILE* bfr = nullptr;
time_t last_backup_time = 0;
char buffer[10000000] = { 0 };				//10M，缓存整个虚拟磁盘文件

std::mutex mutex;  // Mutex to protect shared data
std::condition_variable cv;  // Condition variable for synchronization
std::vector<Client> allClients;  // Vector to hold clients
bool stopProcessing = false;  // Flag to signal threads to stop processing


void client_handler(Client& client) {
    sprintf(client.buffer, "client sock: %d, nextUID is %d, nextGID is %d\n", client.client_sock, nextUID, nextGID);
    send(client.client_sock, client.buffer, strlen(client.buffer), 0);
    Welcome(client);
    handleClient(client);
    close(client.client_sock);
    pthread_exit(NULL);
}

void worker_thread() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);

        // Wait for a client or exit signal
        cv.wait(lock, [] { return !allClients.empty() || stopProcessing; });

        // If there are no clients and we received the exit signal, exit the thread
        if (allClients.empty() && stopProcessing) {
            break;
        }

        // Process the next client
        Client client = allClients.back();
        allClients.pop_back();
        lock.unlock();  // Unlock before calling the client handler

        client_handler(client);
    }
}

int main()
{
    Initializer InitializeFileSys;               // 初始化文件系统
    const int numThreads = 20;  // Adjust this based on your requirements
    std::vector<std::thread> threads;

    // Start worker threads
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker_thread);
    }
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;//ipv4
    server_sockaddr.sin_port = htons(MY_PORT);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(server_sock, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) == -1)
    {//绑定本地ip与端口
        perror("Bind Failure\n");
        printf("Error: %s\n", strerror(errno));//输出错误信息
        return -1;
    }
    printf("Listen Port : %d\n", MY_PORT);
    if (listen(server_sock, MAX_QUEUE_NUM) == -1)
    {
        perror("Listen Error");
        close(server_sock);
        return -1;
    }

    printf("Waiting for connection!\n");
    while (true) {
        Client client;
        client.length = sizeof(client.client_addr);

        client.client_sock = accept(server_sock, (struct sockaddr*)&client.client_addr, &client.length);
        if (client.client_sock == -1) {
            perror("Connect Error");
            return -1;
        }

        // Lock the mutex and add the client to the vector
        {
            std::lock_guard<std::mutex> lock(mutex);
            allClients.push_back(client);
        }

        // Notify one worker thread to process the new client
        cv.notify_one();
    }
    // Signal threads to stop processing
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopProcessing = true;
    }

    // Notify all threads to wake up and check the exit condition
    cv.notify_all();

    // Wait for all worker threads to finish
    for (std::thread& thread : threads) {
        thread.join();
    }
    close(server_sock);//关闭服务器响应socket
    //system("pause");
    fclose(fw);		//释放文件指针
    fclose(fr);		//释放文件指针
    return 0;
}
        