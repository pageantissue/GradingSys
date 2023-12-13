#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<ctime>
#include"snapshot.h"
#include"os.h"

using namespace std;


bool fullBackup() {
	time_t cur_time;
	char backupSysName[256] = { 0 };
	time(&cur_time);
	strftime(backupSysName, sizeof(backupSysName), "FullBackupSys %Y.%m.%d %H-%M-%S.sys", localtime(&cur_time));

	if ((bfr = fopen(backupSysName, "rb")) == NULL) {
		bfw = fopen(backupSysName, "wb");
		if (bfw == NULL) {
			printf("备份文件打开失败！");
			return false;
		}
		bfr = fopen(backupSysName, "rb");
		printf("备份文件打开成功");
	}
	else {
		bfw = fopen(backupSysName, "rb+");
		if (bfw == NULL) {
			printf("备份文件打开失败");
			return false;
		}
	}
	//把备份的空间初始化
	char backup_buf[10000000];
	memset(backup_buf, '\0', sizeof(backup_buf));
	fseek(bfw, Start_Addr, SEEK_SET);
	fwrite(backup_buf, sizeof(backup_buf), 1, bfw);

	try {
		char tmp_backup[10000000];
		fseek(fr, Start_Addr, SEEK_SET);
		fread(&tmp_backup, sizeof(tmp_backup), 1, fr);
		fseek(bfw, Start_Addr, SEEK_SET);
		fwrite(&tmp_backup, sizeof(tmp_backup), 1, bfw);
	}
	catch (exception e) {
		printf("%s\n", e.what());
		return false;
	}
	//清除位图标记
	char tmp_inodeBitmap[INODE_NUM];
	for (int i = 0; i < INODE_NUM; i++) {
		tmp_inodeBitmap[i] = 0;
	}
	fseek(fw, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	fwrite(&tmp_inodeBitmap, sizeof(tmp_inodeBitmap), 1, fw);

	time_t curtime;
	time(&curtime);
	last_backup_time = curtime;
	return true;
}

//增量转存
bool incrementalBackup() {
	//创建新的sys文件
	time_t cur_time;
	char backupSysName[256] = { 0 };
	time(&cur_time);
	strftime(backupSysName, sizeof(backupSysName), "IncreBackupSys %Y.%m.%d %H-%M-%S.sys", localtime(&cur_time));

	if ((bfr = fopen(backupSysName, "rb")) == NULL) {
		bfw = fopen(backupSysName, "wb");
		if (bfw == NULL) {
			printf("备份文件打开失败！");
			return false;;
		}
		bfr = fopen(backupSysName, "rb");
		printf("备份文件打开成功");
	}
	else {
		bfw = fopen(backupSysName, "rb+");
		if (bfw == NULL) {
			printf("备份文件打开失败");
			return false;
		}
	}

	char tmp_inodeBitmap[INODE_NUM];
	memset(tmp_inodeBitmap, '\0', sizeof(tmp_inodeBitmap));
	fseek(fr, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	fread(&tmp_inodeBitmap, sizeof(tmp_inodeBitmap), 1, fr);

	//对每一个修改过的文件&全部目录在位图中做标记
	for (int i = 0; i < INODE_NUM; i++) {
		inode tmp_inode;
		fseek(fr, Inode_Start_Addr, SEEK_SET);
		fread(&tmp_inode, sizeof(inode), 1, fr);
		if (tmp_inode.file_modified_time > last_backup_time) {
			tmp_inodeBitmap[i] = 1;
		}
		else {
			continue;
		}
	}

	fseek(fw, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	fwrite(&tmp_inodeBitmap, sizeof(modified_inode_bitmap), 1, fw);
	
	//在备份文件中先记录modified inode bitmap
	fseek(bfw, Start_Addr, SEEK_SET);
	fwrite(&tmp_inodeBitmap, sizeof(modified_inode_bitmap), 1, bfw);

	//去每一个被修改的inode里找被对应修改的直接块
	int Cur_Addr=Backup_Block_Start_Addr;
	for (int i = 0; i < INODE_NUM; i++) {
		inode tmp_inode;
		if (modified_inode_bitmap[i] == 1) {
			char tmp[128];
			memset(tmp, '\0', sizeof(tmp));
			//读修改过的inode
			fseek(fr, Inode_Start_Addr, SEEK_SET);
			fread(&tmp_inode, sizeof(inode), 1, fr);
			//将修改过的inode写入增量备份系统
			fseek(bfw, Cur_Addr, SEEK_SET);
			fwrite(&tmp_inode, sizeof(inode), 1, bfw);
			Cur_Addr += sizeof(inode);
			//找inode直接块
			int tmp_i_dirBlock[10];
			for (int j = 0; j < 10; j++) {
				if (tmp_inode.i_dirBlock[j] != -1) {
					//存直接块
					char tmp_block[512];
					int block_addr = tmp_inode.i_dirBlock[j];
					fseek(fr, block_addr, SEEK_SET);
					fread(&tmp_block, sizeof(tmp_block), 1, fr);
					fseek(bfw, Cur_Addr, SEEK_SET);
					fwrite(&tmp_block, sizeof(tmp_block), 1, bfw);
					Cur_Addr += sizeof(tmp_block);;
				}
				else {
					continue;
				}
			}

		}
	}
	time_t curtime;
	time(&curtime);
	last_backup_time = curtime;
	return true;
}

bool recovery() {

}