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


extern time_t last_backup_time;

extern FILE* bfw;							//备份文件 写文件指针
extern FILE* bfr;							//备份文件 读文件指针


extern const int Backup_Start_Addr;
extern int Backup_Cur_Addr;			//备份文件系统当前地址
extern const int Backup_Block_Start_Addr;


bool fullBackup();
bool incrementalBackup();
bool recovery();
bool demo();