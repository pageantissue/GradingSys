#pragma once
#include"os.h"
#include<iostream>
#include<cstdio>
#include<ctime>
#include<cstring>

#define BLOCK_SIZE 512	//һ�����С 512 Byte
#define INODE_SIZE 128  //һ��inode entry�Ĵ�С��128Byte
#define DirItem_Size 16 //һ���������װ16��DirItem
#define FILE_NAME_MAX_SIZE	28	//�ļ����28Byte
#define Operation_Num 20		//�����20�β���
#define Backup_Block_Num 43		//����һ��backup�ṹ��ռ��43��block


extern time_t last_backup_time;

extern FILE* bfw;							//�����ļ� д�ļ�ָ��
extern FILE* bfr;							//�����ļ� ���ļ�ָ��


extern const int Backup_Start_Addr;
extern int Backup_Cur_Addr;			//�����ļ�ϵͳ��ǰ��ַ
extern const int Backup_Block_Start_Addr;


bool fullBackup();
bool incrementalBackup();
bool recovery();
bool demo();