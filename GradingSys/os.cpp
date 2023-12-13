#include"os.h"
#include<iomanip>
#include<time.h>
#include<string.h>
#include<stdio.h>
#include<iostream>
#include"snapshot.h"

using namespace std;


//****大类函数****
bool Format(int count) { //ok
	//初始化:超级块,位图
	char buffer[Disk_Size];
	memset(buffer, '\0', sizeof(buffer));
	fseek(fw, 0, SEEK_SET);
	fwrite(buffer, sizeof(buffer), 1, fw);

	superblock->s_INODE_NUM = INODE_NUM;
	superblock->s_BLOCK_NUM = BLOCK_NUM;
	superblock->s_free_INODE_NUM = INODE_NUM;
	superblock->s_free_BLOCK_NUM = BLOCK_NUM;
	superblock->s_BLOCK_SIZE = BLOCK_SIZE;
	superblock->s_INODE_SIZE = INODE_SIZE;
	superblock->s_SUPERBLOCK_SIZE = sizeof(SuperBlock);
	superblock->s_Superblock_Start_Addr = Superblock_Start_Addr;
	superblock->s_InodeBitmap_Start_Addr = InodeBitmap_Start_Addr;
	superblock->s_BlockBitmap_Start_Addr = BlockBitmap_Start_Addr;
	superblock->s_Inode_Start_Addr = Inode_Start_Addr;
	superblock->s_Block_Start_Addr = Block_Start_Addr;
	fseek(fw, Superblock_Start_Addr, SEEK_SET);
	fwrite(superblock, sizeof(SuperBlock), 1, fw);

	memset(inode_bitmap, 0, sizeof(inode_bitmap));
	fseek(fw, InodeBitmap_Start_Addr, SEEK_SET);
	fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fw);

	memset(block_bitmap, 0, sizeof(block_bitmap));
	fseek(fw, BlockBitmap_Start_Addr, SEEK_SET);
	fwrite(block_bitmap, sizeof(block_bitmap), 1, fw);

	//inode和block板块暂时不需要内容
	fflush(fw);//将上面内容放入fw中

	//创建根目录
	int iaddr = ialloc();
	int baddr = balloc();
	if (iaddr == -1 || baddr==-1) {
		return false;
	}

	inode ino;
	ino.inode_id = iaddr;
	ino.inode_mode = MODE_DIR | DIR_DEF_PERMISSION;
	ino.inode_file_count = 1; //"."也算
	strcpy(ino.i_uname , "root");
	strcpy(ino.i_gname , "root");
	ino.inode_file_size = 1 * BLOCK_SIZE;
	time(&ino.inode_change_time);
	time(&ino.dir_change_time);
	time(&ino.file_modified_time);
	// printf("%s", asctime(localtime(&time)));转成合适格式
	memset(ino.i_dirBlock, -1, sizeof(ino.i_dirBlock));
	ino.i_indirect_1 = -1;
	ino.i_indirect_2 = -1;
	ino.i_dirBlock[0] = baddr;
	fseek(fw, iaddr, SEEK_SET);
	fwrite(&ino, sizeof(inode), 1, fw);

	DirItem ditem[DirItem_Size];
	for (int i = 0; i < DirItem_Size; ++i){
		ditem[i].inodeAddr = -1;
		strcpy(ditem[i].itemName, "");
	}
	ditem[0].inodeAddr = iaddr;
	strcpy(ditem[0].itemName , ".");
	fseek(fw, baddr, SEEK_SET);
	fwrite(ditem, sizeof(ditem), 1, fw);

	fflush(fw);
	//创建目录及配置文件
	mkdir(Cur_Dir_Addr, "home");
	cd(Cur_Dir_Addr, "home");
	mkdir(Cur_Dir_Addr, "root");


	//DirItem gitem[DirItem_Size];
	//fseek(fr, 143872, SEEK_SET);
	//fread(gitem, sizeof(ditem), 1, fr);
	
	gotoRoot();
	mkdir(Cur_Dir_Addr, "etc");
	cd(Cur_Dir_Addr, "etc");

	char buf[1000] = { 0 };
	sprintf(buf, "root:%d:%d\n", nextUID++, nextGID++);//root:uid-0,gid-0
	mkfile(Cur_Dir_Addr, "passwd", buf);

	char* pmode = "0400";//owner:可读
	sprintf(buf, "root:root\n");
	mkfile(Cur_Dir_Addr, "shadow", buf);
	chmod(Cur_Dir_Addr, "shadow", pmode);

	sprintf(buf, "root:%d:root\n", ROOT);
	sprintf(buf + strlen(buf), "teacher:%d:\n", TEACHER);
	sprintf(buf + strlen(buf), "student:%d:\n", STUDENT);
	mkfile(Cur_Dir_Addr, "group", buf);
	
	gotoRoot();
	return true;
}

bool Install() {	//安装文件系统 ok
	fseek(fr, Superblock_Start_Addr, SEEK_SET);
	fread(superblock, sizeof(superblock), 1, fr);

	fseek(fr, InodeBitmap_Start_Addr, SEEK_SET);
	fread(inode_bitmap, sizeof(inode_bitmap), 1, fr);

	fseek(fr, BlockBitmap_Start_Addr, SEEK_SET);
	fread(block_bitmap, sizeof(block_bitmap), 1, fr);

	fflush(fr);
	return true;
}



bool mkdir(int PIAddr, char name[]) {	//目录创建函数(父目录权限:读写执行)
	//理论上Cur_Dir_Addr是系统分配的，应该是正确的
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		printf("文件名称超过最大长度\n");
		return false;
	}
	//查找父目录的空位置
	int bpos = -1;
	int dpos = -1;
	int empty_b = -1;
	inode parino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&parino, sizeof(parino), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(Cur_Group_Name, parino.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(Cur_User_Name, parino.i_uname) == 0) {
		role = 6;
	}
	if ((((parino.inode_mode >> role >> 1) & 1 == 0) )|| (strcmp(Cur_User_Name, "root") != 0)) { //可写
		printf("权限不足，无法新建目录\n");
		return false;
	}

	for (int i = 0; i < 10; ++i) {
		int baddr = parino.i_dirBlock[i];
		if (baddr != -1) {//block已被使用 
			DirItem ditem[DirItem_Size];
			fseek(fr, baddr, SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (ditem[j].inodeAddr == -1) {//未使用过的项
					bpos = i;
					dpos = j;
				}
				if (strcmp(ditem[j].itemName, name) == 0) {//判断：存在同名目录
					//inode temp;
					//fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					//fread(&temp, sizeof(inode), 1, fr);
					//if (((temp.inode_mode >> 9) & 1) == 1) {//是目录
					//	printf("该目录下已包含同名目录\n");
					//	return false;
					//}
					//printf("该目录下已包含同名目录或文件\n");
					return false;
				}
			}
		}
	}
	fflush(fr);
	//空闲块倒序存储

	if (bpos == -1 || dpos == -1) {	//block不足要新开
		for (int i = 0; i < 10; ++i) {
			if (parino.i_dirBlock[i] == -1) {	//找到空闲块
				empty_b = i;
				break;
			}
		}
		if (empty_b == -1) {
			printf("该目录已满，无法添加更多文件");
			return false;
		}
		int baddr = balloc();
		if (baddr == -1)	return false;

		parino.i_dirBlock[empty_b] = baddr;
		parino.inode_file_size += BLOCK_SIZE;
		fseek(fw, Cur_Dir_Addr, SEEK_SET);
		fwrite(&parino, sizeof(parino), 1, fw);
		fflush(fw);

		//初始化新的DirItem
		DirItem ditem[DirItem_Size];
		for (int i = 0; i < DirItem_Size; ++i) {
			ditem[i].inodeAddr = -1;
			strcpy(ditem[i].itemName, "");
		}
		fseek(fw, baddr, SEEK_SET);
		fwrite(ditem, sizeof(ditem), 1, fr);

		bpos = empty_b;
		dpos = 0;
	}

	//创建子目录：bpos,dpos
	int chiiaddr = ialloc();
	int chibaddr = balloc();

	//父节点inode和block更新
	parino.inode_file_count += 1;
	time(&parino.inode_change_time);
	time(&parino.dir_change_time);
	fseek(fw, PIAddr, SEEK_SET);
	fwrite(&parino, sizeof(parino), 1, fw);

	DirItem paritem[DirItem_Size];
	fseek(fr, parino.i_dirBlock[bpos], SEEK_SET);
	fread(paritem, sizeof(paritem), 1, fr);
	fflush(fr);
	strcpy(paritem[dpos].itemName, name);
	paritem[dpos].inodeAddr = chiiaddr;
	fseek(fw, parino.i_dirBlock[bpos], SEEK_SET);
	fwrite(paritem, sizeof(paritem), 1, fw);

	//子节点更新
	inode chiino;
	chiino.inode_id = chiiaddr;
	chiino.inode_mode = MODE_DIR | DIR_DEF_PERMISSION;
	chiino.inode_file_count = 2; //"."和".."
	strcpy(chiino.i_uname, Cur_User_Name);
	strcpy(chiino.i_gname, Cur_Group_Name);
	chiino.inode_file_size = 1 * BLOCK_SIZE;
	time(&chiino.inode_change_time);
	time(&chiino.dir_change_time);
	time(&chiino.file_modified_time);
	memset(chiino.i_dirBlock, -1, sizeof(chiino.i_dirBlock));
	chiino.i_indirect_1 = -1;
	chiino.i_indirect_2 = -1;
	chiino.i_dirBlock[0] = chibaddr;
	fseek(fw, chiiaddr, SEEK_SET);
	fwrite(&chiino, sizeof(inode), 1, fw);

	DirItem chiitem[DirItem_Size];
	for (int i = 0; i < DirItem_Size; ++i) {
		chiitem[i].inodeAddr = -1;
		strcpy(chiitem[i].itemName, "");
	}
	chiitem[0].inodeAddr = chiiaddr;
	strcpy(chiitem[0].itemName, ".");
	chiitem[1].inodeAddr = Cur_Dir_Addr;
	strcpy(chiitem[1].itemName, "..");
	fseek(fw, chibaddr, SEEK_SET);
	fwrite(chiitem, sizeof(chiitem), 1, fw);

	fflush(fw);
	DirItem ditem[DirItem_Size];
	//backup(count, 0);
	return true;
}

bool mkfile(int PIAddr, char name[],char buf[]) {	//文件创建函数
	//理论上Cur_Dir_Addr是系统分配的，应该是正确的
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		printf("文件名称超过最大长度\n");
		return false;
	}

	//查找父目录的空位置
	int bpos = -1;
	int dpos = -1;
	int empty_b = -1;
	inode parino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&parino, sizeof(parino), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(Cur_Group_Name, parino.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(Cur_User_Name, parino.i_uname) == 0) {
		role = 6;
	}
	if ((((parino.inode_mode >> role >> 1) & 1 == 0)) || (strcmp(Cur_User_Name, "root") != 0)) {
		printf("权限不足，无法新建目录\n");
		return false;
	}
	
	for (int i = 0; i < 10; ++i) {
		int baddr = parino.i_dirBlock[i];
		if (baddr != -1) {//block已被使用 
			DirItem ditem[DirItem_Size];
			fseek(fr, baddr, SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (ditem[j].inodeAddr == -1) {//未使用过的项
					bpos = i;
					dpos = j;
				}
				if (strcmp(ditem[j].itemName, name) == 0) {//判断：存在同名目录
					printf("该目录下已包含同名目录或文件\n");
					return false;
				}
			}
		}
	}
	//已经说明可以创建文件or目录了
	fflush(fr);

	if (bpos == -1 || dpos == -1) {	//block不足要新开
		for (int i = 0; i < 10; ++i) {
			if (parino.i_dirBlock[i] == -1) {
				empty_b = i;
			}
		}
		if (empty_b == -1) {
			printf("该目录已满，无法添加更多文件");
			return false;
		}
		int baddr = balloc();
		if (baddr == -1)	return false;

		parino.i_dirBlock[empty_b] = baddr;
		parino.inode_file_size += BLOCK_SIZE;
		fseek(fw, Cur_Dir_Addr, SEEK_SET);
		fwrite(&parino, sizeof(parino), 1, fw);
		fflush(fw);

		DirItem ditem[DirItem_Size];
		for (int i = 0; i < DirItem_Size; ++i) {
			ditem[i].inodeAddr = -1;
			strcpy(ditem[i].itemName, "");
		}
		fseek(fw, baddr, SEEK_SET);
		fwrite(ditem, sizeof(ditem), 1, fr);

		bpos = empty_b;
		dpos = 0;
	}

	//创建子目录：bpos,dpos
	int chiiaddr = ialloc();
	int chibaddr = balloc();

	//父节点inode和block更新
	parino.inode_file_count += 1;
	time(&parino.inode_change_time);
	time(&parino.dir_change_time);
	fseek(fw, PIAddr, SEEK_SET);
	fwrite(&parino, sizeof(parino), 1, fw);

	DirItem paritem[DirItem_Size];
	fseek(fr, parino.i_dirBlock[bpos], SEEK_SET);
	fread(paritem, sizeof(paritem), 1, fr);
	fflush(fr);
	strcpy(paritem[dpos].itemName, name);
	paritem[dpos].inodeAddr = chiiaddr;
	fseek(fw, parino.i_dirBlock[bpos], SEEK_SET);
	fwrite(paritem, sizeof(paritem), 1, fw);

	inode chiino;
	chiino.inode_id = chiiaddr;
	chiino.inode_mode = MODE_FILE | FILE_DEF_PERMISSION;
	chiino.inode_file_count = 1; 
	strcpy(chiino.i_uname, Cur_User_Name);
	strcpy(chiino.i_gname, Cur_Group_Name);
	time(&chiino.inode_change_time);
	time(&chiino.dir_change_time);
	time(&chiino.file_modified_time);
	memset(chiino.i_dirBlock, -1, sizeof(chiino.i_dirBlock));
	chiino.i_indirect_1 = -1;
	chiino.i_indirect_2 = -1;
	chiino.i_dirBlock[0] = chibaddr;
	chiino.inode_file_size =0;
	fseek(fw, chiiaddr, SEEK_SET);
	fwrite(&chiino, sizeof(inode), 1, fw);

	writefile(chiino, chiiaddr, buf);//将buf信息写入(新开）
	
	fflush(fw);
	return true;
}
bool rm(int PIAddr, char name[], int type) {	//删除文件or文件夹
	//文件和目录不允许重名
	inode ino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&ino, sizeof(ino), 1, fr);

	for (int i = 0; i < 10; ++i) {
		if (ino.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			fseek(fr, ino.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) { //找到同名
					if (type == 1) {//1:目录
						if (recursive_rmdir(ditem[j].inodeAddr, name)) {	//成功删除
							printf("已经成功删除目录及其子文件!\n");
							strcpy(ditem[j].itemName, "");
							ditem[j].inodeAddr = -1;
							fseek(fw, ino.i_dirBlock[i], SEEK_SET);
							fwrite(ditem, sizeof(ditem), 1, fw);
						}
						else {
							return false;
						}
					}

					if (type == 0) {	//0:文件
						if (recursive_rmfile(ditem[j].inodeAddr, name)) {	//成功删除
							printf("已经成功删除该文件!\n");
							strcpy(ditem[j].itemName, "");
							ditem[j].inodeAddr = -1;

							fseek(fw, ino.i_dirBlock[i], SEEK_SET);
							fwrite(ditem, sizeof(ditem), 1, fw);
						}
						else {
							return false;
						}

					}

					//是否需要释放block
					int flag = 1;
					for (int k = 0; k < DirItem_Size; ++k) {
						if (ditem[j].inodeAddr != -1) {
							flag = 0;//block还在使用
							break;
						}
					}
					if (flag == 0) {
						break;
					}
					else {
						bfree(ino.i_dirBlock[i]);
						ino.i_dirBlock[i] = -1;
						ino.inode_file_size -= BLOCK_SIZE;
					}
				}
			}
		}
	}
	//父节点inode和block更新
	ino.inode_file_count -= 1;
	time(&ino.inode_change_time);
	time(&ino.dir_change_time);
	fseek(fw, PIAddr, SEEK_SET);
	fwrite(&ino, sizeof(ino), 1, fw);
	return true;
}
bool recursive_rmdir(int CHIAddr, char name[]) {//删除当前目录(父亲block和inode处的记录没删，不能直接用！！）
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		printf("文件名称超过最大长度\n");
		return false;
	}
	if ((strcmp(name, ".") == 0) || strcmp(name, "..") == 0 ){
		printf("文件无法删除\n");
		return false;
	}

	//判断权限
	inode ino;
	fseek(fr, CHIAddr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);

	int mode = 0;//other
	if (strcmp(Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 1) & 1) == 0) || (strcmp(Cur_User_Name, "root") != 0)) {//是否可写：2
		printf("没有权限删除该文件夹\n");
		return false;
	}

	//删除其子文件夹
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		if (ino.i_dirBlock[i] != -1) {//被使用过
			fseek(fr, ino.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				inode chiino;
				if (strcmp(ditem[j].itemName, ".") == 0 || strcmp(ditem[j].itemName, "..") == 0) {
					ditem[j].inodeAddr = -1;
					strcpy(ditem[j].itemName, "");
					continue;
				}
				if (strlen(ditem[j].itemName) != 0) {
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&chiino, sizeof(inode), 1, fr);
					if ((chiino.inode_mode >> 9) & 1 == 1) {	//目录
						recursive_rmdir(ditem[j].inodeAddr, ditem[j].itemName);
					}
					else {										//文件
						recursive_rmfile(ditem[j].inodeAddr, ditem[j].itemName);
					}
				}
				ditem[j].inodeAddr = -1;
				strcpy(ditem[j].itemName, "");
			}
			//block清理&inode参数减少
			bfree(ino.i_dirBlock[i]);
			ino.inode_file_count -= 16;
			ino.inode_file_size -= BLOCK_SIZE;
			ino.i_dirBlock[i] = -1;
		}
	}

	ifree(CHIAddr);
	return true;
}
bool recursive_rmfile(int CHIAddr, char name[]) {	//删除当前文件（父亲inode和block处的节点没删，不能直接用）
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		printf("文件名称超过最大长度\n");
		return false;
	}

	//判断权限
	inode ino;
	fseek(fr, CHIAddr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);

	int mode = 0;//other
	if (strcmp(Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 1) & 1) == 0) || (strcmp(Cur_User_Name, "root") != 0)) {//是否可写：2
		printf("没有权限删除该文件\n");
		return false;
	}

	//删除其内容
	for (int i = 0; i < 10; ++i) {
		if (ino.i_dirBlock[i] != -1) {//被使用过
			//block清理&inode参数减少
			bfree(ino.i_dirBlock[i]);
			ino.i_dirBlock[i] = -1;
		}
	}
	ino.inode_file_count = 0;
	ino.inode_file_size =0;
	ifree(CHIAddr);
	return true;
}
bool cat(int PIAddr, char name[]) {	//查看文件内容
	inode parino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&parino, sizeof(parino), 1, fr);

	for (int i = 0; i < 10; ++i) {
		if (parino.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			fseek(fr, parino.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) {	//同名
					inode chiino;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&chiino, sizeof(chiino), 1, fr);
					if(((chiino.inode_mode>>9)&1)==0){//文件
						//读文件内容
						for (int k = 0; k < 10;++k) {
							if (chiino.i_dirBlock[k] != -1) {
								char content[BLOCK_SIZE];
								fseek(fr, chiino.i_dirBlock[k], SEEK_SET);
								fread(content, sizeof(content), 1, fr);
								printf("%s\n", content);
							}	
						}
					}
				}
			}
		}
	}
	
}
bool echo(int PIAddr, char name[], int type, char* buf) {	//文件新增or重写or补全
	buf += 1;
	buf[strlen(buf) - 1] = '\0';
	if (type == 0) {
		if (mkfile(PIAddr, name, buf)) {
			return true;
		}
	}
	inode parino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&parino, sizeof(parino), 1, fr);
	for (int i = 0; i < 10; ++i) {
		if (parino.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			fseek(fr, parino.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) {	//同名
					inode chiino;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&chiino, sizeof(chiino), 1, fr);
					if (((chiino.inode_mode >> 9) & 1) == 0) {//文件
						//0：覆盖写入
						if (type == 0) {
							if (writefile(chiino, ditem[j].inodeAddr, buf)) {
								return true;
							}
							else {
								return false;
							}
						}
						//1：追加
						if (type == 1) {
							if (addfile(chiino, ditem[j].inodeAddr, buf)) {
								return true;
							}
							else {
								return false;
							}
						}
					}
				}
			}
		}
	}
}
bool writefile(inode fileinode, int iaddr, char buf[]) { //文件写入

	int new_block = strlen(buf) / BLOCK_SIZE + 1;
	for (int i = 0; i < new_block; ++i) {
		int baddr = fileinode.i_dirBlock[i];
		if (baddr == -1) {
			baddr = balloc();
		}
		char temp_file[BLOCK_SIZE];
		memset(temp_file, '\0', BLOCK_SIZE);
		if (i == new_block - 1) {
			strcpy(temp_file, buf + BLOCK_SIZE * i);//buf+blocksize*i-->buf+blocksize*i+1
		}
		else {
			strncpy(temp_file, buf + BLOCK_SIZE * i, BLOCK_SIZE);
		}

		fseek(fw, baddr, SEEK_SET);
		fwrite(temp_file, BLOCK_SIZE, 1, fw);

		fileinode.i_dirBlock[i] = baddr;
	}
	fileinode.inode_file_size = strlen(buf);
	time(&fileinode.inode_change_time);
	time(&fileinode.file_modified_time);
	fseek(fw, iaddr, SEEK_SET);
	fwrite(&fileinode, sizeof(fileinode), 1, fw);
	return true;
}
bool addfile(inode fileinode, int iaddr, char buf[]) { //文件续写ok
	//前提：假设是按照block顺序存储
	if ((fileinode.inode_file_size + strlen(buf)) > 10 * BLOCK_SIZE) {
		printf("文件内存不足，无法继续添加内容\n");
		return false;
	}

	fileinode.inode_file_size += strlen(buf);
	time(&fileinode.inode_change_time);
	time(&fileinode.file_modified_time);
	fseek(fw, iaddr, SEEK_SET);
	fwrite(&fileinode, sizeof(inode), 1, fw);

	//写入文件
	int start = fileinode.inode_file_size / BLOCK_SIZE; //使用到第几块（考虑block[0])
	char temp[BLOCK_SIZE];
	for (int i = start; i < 10; ++i) {
		if (fileinode.i_dirBlock[i] != -1) {	//正在使用块(补全）
			fseek(fr, fileinode.i_dirBlock[i], SEEK_SET);
			fread(temp, BLOCK_SIZE, 1, fr);
			fflush(fr);
			int offset = BLOCK_SIZE - strlen(temp);
			strncat(temp, buf, offset);
			fseek(fw, fileinode.i_dirBlock[i], SEEK_SET);
			fwrite(temp, BLOCK_SIZE, 1, fw);
			fflush(fw);
			if (strlen(buf) > offset) {
				buf = buf + offset;
			}
			else {
				break;
			}
			
		}
		else {
			int baddr = balloc();
			if (baddr == -1) return false;
			fileinode.i_dirBlock[i] = baddr;
			fseek(fw, fileinode.i_dirBlock[i], SEEK_SET);
			fwrite(buf, BLOCK_SIZE, 1, fw);
			fflush(fw);
			if (strlen(buf) > BLOCK_SIZE) {	//还有没放完的
				buf += BLOCK_SIZE;
			}
			else {
				break;
			}
		}
	}

	fflush(fw);
	return true;
}
bool cd(int PIAddr, char name[]) {//切换目录

	inode pinode;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&pinode, sizeof(inode), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(Cur_Group_Name, pinode.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(Cur_User_Name, pinode.i_uname) == 0) {
		role = 6;
	}

	for (int i = 0; i < 10; ++i) {
		if (pinode.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			fseek(fr, pinode.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) { //找到同名
					if (strcmp(name, ".") == 0) {
						return true;
					}
					if (strcmp(name, "..") == 0) {
						if (strcmp(Cur_Dir_Name, "/") ==0){
							return true;
						}
						//char* p = strrchr(Cur_Dir_Addr, '/'); 跑不了啊
						char* p = Cur_Dir_Name+strlen(Cur_Dir_Name);
						while ((*p) != '/')p--;
						*p = '\0'; //打断它
						Cur_Dir_Addr = ditem[j].inodeAddr;
						return true;
					}
					inode chiino;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&chiino, sizeof(inode), 1, fr);
					fflush(fr);
					if (((chiino.inode_mode >> role) & 1) == 1) {	//是否有执行权限
						if (strcmp(Cur_Dir_Name, "/") != 0) {
							strcat(Cur_Dir_Name, "/");
						}
						strcat(Cur_Dir_Name, name);
						Cur_Dir_Addr = ditem[j].inodeAddr;
						return true;
					}
				}
			}
		}
	}
	printf("该文件夹不存在，无法进入\n");
	return false;
}
void gotoRoot() { //ok
	Cur_Dir_Addr= Root_Dir_Addr;
	strcpy(Cur_Dir_Name , "/");
}
void ls(char str[]) {//显示当前目录所有文件 ok
	inode ino;
	fseek(fr, Cur_Dir_Addr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);
	fflush(fr);
	//printf("%s\n", ino);
	
	//查看权限
	int mode = 0;//other
	if (strcmp(Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 2) & 1 )== 0) &&(strcmp(Cur_User_Name, "root") != 0)) {//是否可读：4
		printf("没有权限查看该文件夹\n");
		return;
	}
	
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		if (ino.i_dirBlock[i] != -1) {//被使用过
			fseek(fr, ino.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			if (strcmp(str, "-l") == 0) {
				//取出目录项的inode
				
				for (int j = 0; j < DirItem_Size; j++) {
					inode tmp;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&tmp, sizeof(inode), 1, fr);
					fflush(fr);

					if (strcmp(ditem[j].itemName, "") == 0|| (strcmp(ditem[j].itemName, ".") == 0) || (strcmp(ditem[j].itemName, "..") == 0)) {
						continue;
					}

					if (((tmp.inode_mode >> 9) & 1) == 1) {
						printf("d");
					}
					else {
						printf("-");
					}
					
					//权限
					int count = 8;
					while (count >= 0) {
						if (((tmp.inode_mode >> count) & 1) == 1) {
							int mod = count % 3;
							switch (mod) {
							case 0:
								printf("x");
								break;
							case 1:
								printf("w");
								break;
							case 2:
								printf("r");
								break;	
							}
						}
						else {
							printf("-");
						}
						count--;
					}
					printf("\t");
					printf("%s\t", tmp.i_uname);
					printf("%s\t", tmp.i_gname);
					//printf("%s\t", tmp.inode_file_size);
					printf("%s\t", ctime(&tmp.file_modified_time));
					printf("%s\t", ditem[j].itemName);
					printf("\n");
				}
			}
			else {
					for (int j = 0; j < DirItem_Size; ++j) {
						if (strlen(ditem[j].itemName) != 0) {
							if ((strcmp(ditem[j].itemName, ".") == 0) || (strcmp(ditem[j].itemName, "..") == 0))
								continue;
							printf("%s\n", ditem[j].itemName);
						}
					}
			}
		}
	}
	return;
}

//****工具函数****
int ialloc() { //分配inode，满了返回-1 ok
	int iaddr = -1;
	for (int i = 0; i < INODE_NUM; i++) {
		if (inode_bitmap[i] == 0) {
			iaddr = i;
			inode_bitmap[i] = 1;
			break;
		}
	}
	if (iaddr == -1) {
		printf("没有inode空间\n");
		return -1;
	}
	iaddr =Inode_Start_Addr + iaddr * INODE_SIZE;
	superblock->s_free_INODE_NUM -= 1;
	fseek(fw, Superblock_Start_Addr, SEEK_SET);
	fwrite(superblock, sizeof(superblock), 1, fw);
	fseek(fw, InodeBitmap_Start_Addr, SEEK_SET);
	fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fw);
	return iaddr;
}
void ifree(int iaddr) {
	if ((iaddr % INODE_SIZE) != 0) {
		printf("当前inode位置错误\n");
		return;
	}
	int index = (iaddr - Inode_Start_Addr) / INODE_SIZE;
	if (inode_bitmap[index] == 0) {
		printf("未使用当前inode，无需释放\n");
		return;
	}
	inode_bitmap[index] = 0;
	fseek(fw, InodeBitmap_Start_Addr, SEEK_SET);
	fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fw);
	inode ino;
	fseek(fw, iaddr, SEEK_SET);
	fwrite(&ino, sizeof(inode), 1, fw);
	superblock->s_free_INODE_NUM -= 1;
	fseek(fw, Superblock_Start_Addr, SEEK_SET);
	fwrite(superblock, sizeof(superblock), 1, fw);
}
int balloc() { //分配block，满了返回-1 ok
	int baddr = -1;
	int index = -1;
	for (int i = 0; i < BLOCK_NUM; i++) {
		if (block_bitmap[i] == 0) {
			index= i;
			block_bitmap[i] = 1;
			break;
		}
	}
	if (index == -1) {
		printf("没有block空间\n");
		return -1;
	}
	baddr = Block_Start_Addr + index * BLOCK_SIZE;
	superblock->s_free_BLOCK_NUM -= 1;
	fseek(fw, Superblock_Start_Addr, SEEK_SET);
	fwrite(superblock, sizeof(superblock), 1, fw);
	fseek(fw, BlockBitmap_Start_Addr, SEEK_SET);
	fwrite(block_bitmap, sizeof(block_bitmap), 1, fw);
	return baddr;
}
void bfree(int baddr) {
	if ((baddr % BLOCK_SIZE) != 0) {
		printf("当前block位置错误\n");
		return;
	}
	int index = (baddr - Block_Start_Addr) / BLOCK_SIZE;
	if (block_bitmap[index] == 0) {
		printf("未使用当前block，无需释放\n");
		return;
	}
	block_bitmap[index] = 0;
	fseek(fw, BlockBitmap_Start_Addr, SEEK_SET);
	fwrite(block_bitmap, sizeof(block_bitmap), 1, fw);
	superblock->s_free_BLOCK_NUM -= 1;
	fseek(fw, Superblock_Start_Addr, SEEK_SET);
	fwrite(superblock, sizeof(superblock), 1, fw);
}

//****用户&用户组函数****
void inUsername(char* username)	//输入用户名
{
	printf("username:\n");
	scanf("%s", username);	//用户名
}
 
void inPasswd(char *passwd)	//输入密码
{
	printf("password:\n");
	scanf("%s", passwd);
}
void ingroup(char* group) {
	printf("group:(root;teacher;student)\n");
	scanf("%s", group);
}
bool login()	//登陆界面
{	
	//DirItem ditem[DirItem_Size];
	//fseek(fr,143872, SEEK_SET);
	//fread(ditem, sizeof(ditem), 1, fr);

	char username[100] = { 0 };
	char passwd[100] = { 0 };
	inUsername(username);	//输入用户名
	inPasswd(passwd);		//输入用户密码
	if (check(username, passwd)) {			//核对用户名和密码

		isLogin = true;
		return true;
	}
	else {
		isLogin = false;
		return false;
	}
}

bool logout() {	//用户注销
	gotoRoot();
	strcmp(Cur_User_Name, "");
	strcmp(Cur_Group_Name, "");
	strcmp(Cur_User_Dir_Name, "");
	isLogin = false;
	printf("账户注销成功！\n");
	return true;
	//pause
}
bool useradd(char username[], char passwd[], char group[]) {	//用户注册
	//权限判断
	if (strcmp(Cur_User_Name, "root") != 0) {
		printf("权限不足，无法添加用户！\n");
		return false;
	}
	//保护现场并更改信息
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, Cur_Dir_Name);
	strcpy(pro_cur_user_name, Cur_User_Name);
	strcpy(pro_cur_group_name, Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, Cur_User_Dir_Name);

	
	//创建用户目录

	gotoRoot();
	cd(Cur_Dir_Addr, "home");
	mkdir(Cur_Dir_Addr, username);

	//更改文件所有者&更改文件者
	//char name[100];
	//strcpy(Cur_User_Name, username);
	//strcpy(Cur_Group_Name, group);
	//sprintf(name, "/home/%s", username);
	//strcpy(Cur_User_Dir_Name, name);

	//获取etc三文件
	inode etcino,shadowino,passwdino,groupino;
	int shadowiddr, passwdiddr,groupiddr;
	gotoRoot();
	cd(Cur_Dir_Addr, "etc");
	fseek(fr, Cur_Dir_Addr, SEEK_SET);
	fread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			fseek(fr, baddr, SEEK_SET);
			fread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "passwd") == 0) {
					passwdiddr = ditem[j].inodeAddr;
					fseek(fr, passwdiddr, SEEK_SET);
					fread(&passwdino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					fseek(fr, shadowiddr, SEEK_SET);
					fread(&shadowino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					fseek(fr,groupiddr, SEEK_SET);
					fread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//读取三文件内容并修改三文件
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	int g = -1;
	if (strcmp(group, "root")==0) {
		g = 0;
	}
	else if (strcmp(group, "teacher")==0) {
		g = 1;
	}
	else if (strcmp(group, "student")==0) {
		g = 2;
	}
	else {
		printf("用户组别不正确，请重新输入");
		return false;
	}

	//passwd
	memset(buf, '\0', sizeof(buf));
	for (int i = 0; i < 10; ++i) {
		if (passwdino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	//buf[strlen(buf)] = '\0'; (strcat可能会自动添加？）
	if (strstr(buf, username)!= NULL) {
		printf("该用户名已存在\n");
		return false;
	}
	sprintf(buf + strlen(buf), "%s:%d:%d\n", username, nextUID++, g);
	passwdino.inode_file_size = strlen(buf);
	writefile(passwdino, passwdiddr, buf);

	char t[BLOCK_SIZE];
	fseek(fr, passwdino.i_dirBlock[0], SEEK_SET);
	fread(t, BLOCK_SIZE, 1, fr);//不知道能否成功
	fflush(fr);

	//shadow
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	sprintf(buf + strlen(buf), "%s:%s\n", username, passwd);
	shadowino.inode_file_size = strlen(buf);
	writefile(shadowino, shadowiddr, buf);

	fseek(fr, shadowino.i_dirBlock[0], SEEK_SET);
	fread(t, BLOCK_SIZE, 1, fr);
	fflush(fr);

	//group(root:0:XX,XX)
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (groupino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	//拼接状增加
	if (g == 0) {	//root
		char* p = strstr(buf, "teacher");
		char temp[strlen(p) + 1];
		strncpy(temp, p, strlen(p));
		temp[sizeof(temp) - 1] = '\0';
		*p = '\0';
		if (buf[strlen(buf) - 2] == ':') {
			sprintf(buf + strlen(buf) - 1, "%s\n", username);
		}
		else {
			sprintf(buf + strlen(buf) - 1, ",%s\n", username);
		}
		strcat(buf, temp);

	}
	else if (g == 1) {//teacher
		char* p = strstr(buf, "student");
		char temp[strlen(p)+1];
		strncpy(temp, p, strlen(p));
		temp[sizeof(temp) - 1] = '\0';
		*p = '\0';
		if (buf[strlen(buf) - 2] == ':') {
			sprintf(buf + strlen(buf) - 1, "%s\n", username);
		}
		else {
			sprintf(buf + strlen(buf) - 1, ",%s\n", username);
		}
		strcat(buf, temp);
	}
	else {//student
		if (buf[strlen(buf) - 2] == ':') {
			sprintf(buf + strlen(buf) - 1, "%s\n", username);
		}
		else {
			sprintf(buf + strlen(buf) - 1, ",%s\n", username);
		}
	}
	groupino.inode_file_size = strlen(buf);
	writefile(groupino, groupiddr, buf);

	fseek(fr, groupino.i_dirBlock[0], SEEK_SET);
	fread(t, BLOCK_SIZE, 1, fr);//不知道能否成功
	fflush(fr);

	Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(Cur_Dir_Name, pro_cur_dir_name);
	return true;
}
bool userdel(char username[]) {	//用户删除
	if (strcmp(Cur_User_Name, "root") != 0) {
		printf("权限不足，无法删除用户\n");
		return false;
	}
	if (strcmp(username, "root") == 0) {
		printf("无法删除管理员\n");
		return false;
	}
	//保护现场并更改信息
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	strcpy(pro_cur_dir_name, Cur_Dir_Name);
	//char pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	//strcpy(pro_cur_user_name, Cur_User_Name);
	//strcpy(pro_cur_group_name, Cur_Group_Name);
	//strcpy(pro_cur_user_dir_name, Cur_User_Dir_Name);

	//strcpy(Cur_User_Name, username);
	//strcpy(Cur_Group_Name, "");

	//获取etc三文件
	inode etcino, shadowino, passwdino, groupino;
	int shadowiddr, passwdiddr, groupiddr;
	gotoRoot();
	cd(Cur_Dir_Addr, "etc");
	fseek(fr, Cur_Dir_Addr, SEEK_SET);
	fread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			fseek(fr, baddr, SEEK_SET);
			fread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "passwd") == 0) {
					passwdiddr = ditem[j].inodeAddr;
					fseek(fr, passwdiddr, SEEK_SET);
					fread(&passwdino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					fseek(fr, shadowiddr, SEEK_SET);
					fread(&shadowino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					fseek(fr, groupiddr, SEEK_SET);
					fread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//读取三文件内容并修改三文件
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];

	//passwd
	for (int i = 0; i < 10; ++i) {
		if (passwdino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	//buf[strlen(buf)] = '\0'; (strcat可能会自动添加？）
	char* p = strstr(buf, username);
	if (strstr(buf, username) == NULL) {
		printf("该用户名不存在，无法删除\n");
		return false;
	}
	*p = '\0';
	while ((*p) != '\n') {
		p++;
	}
	p++;
	strcat(buf, p);
	passwdino.inode_file_size = strlen(buf);
	writefile(passwdino, passwdiddr, buf);
	//buf即使是好的也是补充写入

	//shadow
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	p = strstr(buf, username);
	*p = '\0';
	while ((*p) != '\n') {
		p++;
	}
	p++;
	strcat(buf, p);
	shadowino.inode_file_size = strlen(buf);
	writefile(shadowino, shadowiddr, buf);

	//group(root:0:XX,XX)
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (groupino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	p = strstr(buf, username);
	if ((*(p - 1)) == ':') {	//第一个，后面空格和逗号都要去掉
		*p = '\0';
		while (((*p) != '\n')&&((*p)!=',')) { 
			p++;
		}
		p++;
	}
	else {	//不是第一个，前面逗号要去掉
		p = p - 1;
		*p = '\0';
		while (((*p) != '\n') && ((*p) != ',')) {
			p++;
		}
	}
	strcat(buf, p);
	groupino.inode_file_size = strlen(buf);
	writefile(groupino, groupiddr, buf);

	gotoRoot();
	cd(Cur_Dir_Addr, "home");
	rm(Cur_Dir_Addr, username,1);	//找到父目录home即可进入

	//恢复现场
	if ((p=strstr(pro_cur_dir_name, username)) == NULL) { //原路径不包含删除文件夹
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
	}
	else {	//包含删除文件夹
		*(--p) = '\0';
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
	}
	printf("用户删除成功!\n");

	return true;
}
bool check(char username[], char passwd[]) {//核验身份登录&设置 ok
	//获取三文件
	inode etcino, shadowino, passwdino, groupino;
	int shadowiddr, passwdiddr, groupiddr;
	gotoRoot();
	cd(Cur_Dir_Addr, "etc");
	fseek(fr, Cur_Dir_Addr, SEEK_SET);
	fread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			fseek(fr, baddr, SEEK_SET);
			fread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "passwd") == 0) {
					passwdiddr = ditem[j].inodeAddr;
					fseek(fr, passwdiddr, SEEK_SET);
					fread(&passwdino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					fseek(fr, shadowiddr, SEEK_SET);
					fread(&shadowino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					fseek(fr, groupiddr, SEEK_SET);
					fread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}
	fflush(fr);
	//读取三文件内容并修改三文件
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	char checkpw[100];
	char group[10];


	//shadow
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcpy(buf, temp);
		}
	}
	char* p = strstr(buf, username);
	if (p == NULL) {
		printf("该用户不存在。请创建用户后重新登陆.\n");
		return false;
	}
	while ((*p) != ':') {
		p++;
	}
	p++;
	int i = 0;
	while ((*p) != '\n') {
		checkpw[i++] = (*p);
		p++;
	}
	if (strcmp(checkpw, passwd) != 0) {
		printf("密码不正确，请重新尝试！\n");
		return false;
	}

	//passwd
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (passwdino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			fseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcpy(buf, temp);
		}
	}
	p = strstr(buf, username);
	int flag;
	i=flag = 0;
	
	memset(group, '\0', strlen(group));
	while ((*p) != '\n') {
		if (flag == 2) {
			group[i++] = (*p);
		}
		if ((*p) == ':') {
			flag++;
		}
		p++;
	}

	//成功登录后的设置
	if(strcmp(group,"0")==0){
		strcpy(Cur_Group_Name, "root");
	}
	else if (strcmp(group, "1") == 0) {
		strcpy(Cur_Group_Name, "teacher");
	}
	else if (strcmp(group, "2") == 0) {
		strcpy(Cur_Group_Name, "student");
	}
	strcpy(Cur_User_Name,username);
	sprintf(Cur_User_Dir_Name, "/home/%s", username);
	gotoRoot();
	cd(Cur_Dir_Addr, "home");
	cd(Cur_Dir_Addr, username);

	return true;
}
bool chmod(int PIAddr, char name[], char* pmode) {//修改文件or目录权限（假定文件和目录也不能重名）
	if (strlen(name) > FILENAME_MAX) {
		printf("文件名称超过最大长度\n");
		return false;
	}
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		printf("该文件无法修改权限\n");
		return false;
	}
	inode ino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);
	fflush(fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		fseek(fr, ino.i_dirBlock[i], SEEK_SET);
		fread(ditem, sizeof(ditem), 1, fr);
		for (int j = 0; j < DirItem_Size; ++j) {
			if (strcmp(ditem[j].itemName, name) == 0) {//找到同名文件
				inode chiino;
				fseek(fr, ditem[j].inodeAddr, SEEK_SET);
				fread(&chiino, sizeof(inode), 1, fr);
				fflush(fr);
				//1:目录 0：文件
				//if (((chiino.inode_mode >> 9) & 1 )!= type) {	//未找到同一类型文件
				//	continue;
				//}
				//只有创建者和管理员可以更改权限
				if ((strcmp(chiino.i_uname, Cur_User_Name) == 0) || strcmp(Cur_User_Name, "root") == 0) {
					unsigned short i_mode = chiino.inode_mode;

					//修改权限
					long int  mode = 0;
					if ((pmode[0] >= '0') && (pmode[0] <= '9')) {	//pmode是数字
						char* endptr;
						mode = strtol(pmode, &endptr, 8);
						if (strlen(endptr)!=0) {
							printf("请使用正确的八进制数字\n");
							return false;
						}
						i_mode = (i_mode >> 9 << 9) | mode;
					}
					else {	//pmode是字母
						char* p;
						char permit[20];
						memset(permit, '\0', sizeof(permit));
						p = strstr(pmode, ",");
						while (strlen(pmode) != 0) {
							if (p != NULL) {
								strncpy(permit, pmode, p - pmode);
								pmode = p + 1;
								p = strstr(pmode, ",");
							}
							else {
								strcpy(permit, pmode);
								pmode = pmode + strlen(pmode);
							}

							//r,w,x
							int symbol = 0;
							char s_symbol = permit[strlen(permit) - 1];
							if (s_symbol == 'r') {
								symbol = 2;//100
							}
							else if (s_symbol == 'w') {
								symbol = 1;//010
							}
							else if (s_symbol == 'x') {
								symbol = 0;//001
							}
							else {
								printf("命令格式错误\n");
								return false;
							}

							//+,-,=
							unsigned int opo = -1;
							char s_opo = permit[strlen(permit) - 2];
							if ((s_opo == '+') || (s_opo == '=')) {
								opo = 1;
							}
							else if (s_opo == '-') {
								opo = 0;
							}
							else {
								printf("命令格式错误\n");
								return false;
							}

							//u,g,o,a
							permit[strlen(permit) - 2] = '\0';
							for (int i = 0; i < strlen(permit); ++i) {
								char s_user = permit[i];
								if ((s_user != 'u') && (s_user != 'g') && (s_user != 'o') && (s_user != 'a')) {
									printf("命令格式错误\n");
									return false;
								}

								if (opo == 1) {//将imode对应位置设为1
									switch (s_user) {
									case 'u':
										i_mode = i_mode | (1 << symbol << 6);
										break;
									case 'g':
										i_mode = i_mode | (1 << symbol << 3);
										break;
									case 'o':
										i_mode = i_mode | (1 << symbol);
										break;
									default:
										i_mode = i_mode | (1 << symbol << 6) | (1 << symbol << 3) | (1 << symbol);
									}
								}
								else {
									switch (s_user) {
									case 'u':
										i_mode = i_mode &~ (1 << symbol << 6);
										break;
									case 'g':
										i_mode = i_mode & ~(1 << symbol << 3);
										break;
									case 'o':
										i_mode = i_mode & ~(1 << symbol);
										break;
									default:
										i_mode = i_mode & (~(1 << symbol << 6)) & (~(1 << symbol << 3)) & (~(1 << symbol));
									}
								}
							}
						}
					}
					//chiino.inode_mode = (chiino.inode_mode >> 9 << 9) | pmode;
					chiino.inode_mode = i_mode;
					fseek(fw, ditem[j].inodeAddr, SEEK_SET);
					fwrite(&chiino, sizeof(inode), 1, fw);
					fflush(fw);
					return true;
				}
				else {
				printf("权限不足\n");
				return false;
				}
			}
		}
	}
	printf("没有找到该文件，无法修改权限\n");
	return false;
}
//bool check_group(char name[], char s_group[]) { //验证name和group是否存在和匹配
//	//判断组别是否存在
//	if ((strcmp(s_group, "root") != 0) && (strcmp(s_group, "teacher") != 0) && (strcmp(s_group, "student") != 0)) {
//		printf("组别不正确\n");
//		return false;
//	}
//
//	//查看passwd文件
//	inode etcino, passwdino;
//	int passwdiddr;
//	char buf[BLOCK_SIZE * 10]; //1char:1B
//	char temp[BLOCK_SIZE];
//	char group[100];
//	gotoRoot();
//	cd(Cur_Dir_Addr, "etc");
//	fseek(fr, Cur_Dir_Addr, SEEK_SET);
//	fread(&etcino, sizeof(inode), 1, fr);
//	for (int i = 0; i < 10; ++i) {
//		DirItem ditem[DirItem_Size];
//		int baddr = etcino.i_dirBlock[i];
//		if (baddr != -1) {
//			fseek(fr, baddr, SEEK_SET);
//			fread(&ditem, BLOCK_SIZE, 1, fr);
//			for (int j = 0; j < DirItem_Size; ++j) {
//				if (strcmp(ditem[j].itemName, "passwd") == 0) {
//					passwdiddr = ditem[j].inodeAddr;
//					fseek(fr, passwdiddr, SEEK_SET);
//					fread(&passwdino, sizeof(inode), 1, fr);
//				}
//			}
//		}
//	}
//
//	memset(buf, '\0', sizeof(temp));
//	for (int i = 0; i < 10; ++i) {
//		if (passwdino.i_dirBlock[i] != -1) {
//			memset(temp, '\0', sizeof(temp));
//			fseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
//			fread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
//			strcpy(buf, temp);
//		}
//	}
//
//	//判断用户是否存在&组别是否匹配
//	char* p = strstr(buf, name);
//	if (p == NULL) {
//		printf("该用户不存在。请重新输入命令.\n");
//		return false;
//	}
//	int flag,i;
//	i = flag = 0;
//	memset(group, '\0', strlen(group));
//	while ((*p) != '\n') {
//		if (flag == 2) {
//			group[i++] = (*p);
//		}
//		if ((*p) == ':') {
//			flag++;
//		}
//		p++;
//	}
//	if (strcmp(group, s_group) == 0) {
//		return true;
//	}
//	return false;
//}
bool chown(int PIAddr,char* filename, char name[], char group[]) {//修改文件所属用户和用户组
	//判断
	if (strlen(filename) > FILENAME_MAX) {
		printf("文件名称超过最大长度\n");
		return false;
	}
	if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
		printf("该文件无法修改权限\n");
		return false;
	}
	if ((strcmp(group, "root") != 0) && (strcmp(group, "teacher") != 0) && (strcmp(group, "student") != 0)) {
		printf("组别不正确！请重新输入！\n");
		return false;
	}

	inode ino;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);
	fflush(fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		fseek(fr, ino.i_dirBlock[i], SEEK_SET);
		fread(ditem, sizeof(ditem), 1, fr);
		for (int j = 0; j < DirItem_Size; ++j) {
			if (strcmp(ditem[j].itemName, filename) == 0) {//找到同名文件
				inode chiino;
				fseek(fr, ditem[j].inodeAddr, SEEK_SET);
				fread(&chiino, sizeof(inode), 1, fr);
				fflush(fr);
				//1:目录 0：文件
				//if (((chiino.inode_mode >> 9) & 1 )!= type) {	//未找到同一类型文件
				//	continue;
				//}
				//只有创建者和管理员可以更改用户组or用户
				if ((strcmp(chiino.i_uname, Cur_User_Name) == 0) || strcmp(Cur_User_Name, "root") == 0) {
					if (strlen(name) != 0) { strcpy(chiino.i_uname, name); }
					if (strlen(group) != 0) { strcpy(chiino.i_gname, group); }
					fseek(fw, ditem[j].inodeAddr, SEEK_SET);
					fwrite(&chiino, sizeof(inode), 1, fw);
					fflush(fw);
					return true;
				}
				else {
					printf("权限不足\n");
					return false;
				}
			}
		}
	}
	printf("没有找到该文件，无法修改权限\n");
	return false;
}


void backup() {
	inode pinode, childinode;

}