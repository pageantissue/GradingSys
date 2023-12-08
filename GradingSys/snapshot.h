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

#define BACKUP_SYS_NAME "backup_sys.sys"	//备份系统名

struct Backup {
	char inodeBitMap[1024];		//占2个blcok
	char blcokBitMap[10240];	//占20个block
	inode childinode;
	inode parinode;
	char childblocks[BLOCK_SIZE * 10];
	char parblocks[BLOCK_SIZE * 10];
};

extern FILE* bfw;							//备份文件 写文件指针
extern FILE* bfr;							//备份文件 读文件指针

extern const int Backup_Cur_Addr;			//备份文件系统当前地址


void backup();