#pragma once
#include"os.h"
#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string.h>

#define BLOCK_SIZE 512	//一个块大小 512 Byte
#define INODE_SIZE 128  //一个inode entry的大小是128Byte
#define DirItem_Size 16 //一个块最多能装16个DirItem
#define FILE_NAME_MAX_SIZE	28	//文件名最长28Byte
#define Operation_Num 20		//假设存20次操作
#define Backup_Block_Num 43		//假设一个backup结构体占用43个block

#define BACKUP_SYS_NAME "backup_sys.sys"	//备份系统名


//struct Backup {
//	char inodeBitMap[1024];		//占2个blcok
//	char blcokBitMap[10240];	//占20个block
//	inode childinode;
//	inode parinode;
//	int childinodeAddr;
//	int parinodeAddr;
//	char childblocks[BLOCK_SIZE * 10];
//	char parblocks[BLOCK_SIZE * 10];
//};

extern time_t last_backup_time;

extern FILE* bfw;							//备份文件 写文件指针
extern FILE* bfr;							//备份文件 读文件指针


extern const int Backup_Start_Addr;
extern const int Backup_Cur_Addr;			//备份文件系统当前地址
extern const int Backup_Block_Start_Addr;

//extern const char backup_buf[500000];
//extern const time_t full_backuptime;

bool fullBackup();
bool incrementalBackup();
bool recovery();