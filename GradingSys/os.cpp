#include"os.h"
#include"server.h"
#include<mutex>
#include<ctime>
#include<cstring>
#include<cstdio>
#include<cstdlib>

using namespace std;


//****大类函数****
bool Format() { //ok
	//初始化:超级块,位图
	char buffer[Disk_Size];
	memset(buffer, '\0', sizeof(buffer));
	safeFseek(fw, 0, SEEK_SET);
	safeFwrite(buffer, sizeof(buffer), 1, fw);

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
	safeFseek(fw, Superblock_Start_Addr, SEEK_SET);
	safeFwrite(superblock, sizeof(SuperBlock), 1, fw);

	memset(inode_bitmap, 0, sizeof(inode_bitmap));
	safeFseek(fw, InodeBitmap_Start_Addr, SEEK_SET);
	safeFwrite(inode_bitmap, sizeof(inode_bitmap), 1, fw);

	memset(block_bitmap, 0, sizeof(block_bitmap));
	safeFseek(fw, BlockBitmap_Start_Addr, SEEK_SET);
	safeFwrite(block_bitmap, sizeof(block_bitmap), 1, fw);

	memset(modified_inode_bitmap, 0, sizeof(modified_inode_bitmap));
	safeFseek(fw, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	safeFwrite(modified_inode_bitmap, sizeof(modified_inode_bitmap), 1, fw);
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
	safeFseek(fw, iaddr, SEEK_SET);
	safeFwrite(&ino, sizeof(inode), 1, fw);

	DirItem ditem[DirItem_Size];
	for (int i = 0; i < DirItem_Size; ++i){
		ditem[i].inodeAddr = -1;
		strcpy(ditem[i].itemName, "");
	}
	ditem[0].inodeAddr = iaddr;
	strcpy(ditem[0].itemName , ".");
	safeFseek(fw, baddr, SEEK_SET);
	safeFwrite(ditem, sizeof(ditem), 1, fw);

	fflush(fw);
	//创建目录及配置文件
	mkdir(sys, sys.Cur_Dir_Addr, "home");
	cd(sys, sys.Cur_Dir_Addr, "home");
	mkdir(sys, sys.Cur_Dir_Addr, "root");


	//DirItem gitem[DirItem_Size];
	//safeFseek(fr, 143872, SEEK_SET);
	//safeFread(gitem, sizeof(ditem), 1, fr);
	
	gotoRoot(sys);
	mkdir(sys, sys.Cur_Dir_Addr, "etc");
	cd(sys, sys.Cur_Dir_Addr, "etc");

	char buf[1000] = { 0 };
	sprintf(buf, "root:%d:%d\n", nextUID++, nextGID);//root:uid-0,gid-0
	mkfile(sys, sys.Cur_Dir_Addr, "passwd", buf);

	char* pmode = "0400";//owner:可读
	sprintf(buf, "root:root\n");
	mkfile(sys, sys.Cur_Dir_Addr, "shadow", buf);
	chmod(sys, sys.Cur_Dir_Addr, "shadow", pmode);

	sprintf(buf, "root:%d:root\n", nextGID++);
	sprintf(buf + strlen(buf), "teacher:%d:\n", nextGID++);
	sprintf(buf + strlen(buf), "student:%d:\n", nextGID++);
	mkfile(sys, sys.Cur_Dir_Addr, "group", buf);
	
	gotoRoot(sys);
	return true;
}

bool Install() {	//安装文件系统 ok
	fseek(fr, Superblock_Start_Addr, SEEK_SET);
	safeFread(superblock, sizeof(superblock), 1, fr);

	fseek(fr, InodeBitmap_Start_Addr, SEEK_SET);
	safeFread(inode_bitmap, sizeof(inode_bitmap), 1, fr);

	fseek(fr, BlockBitmap_Start_Addr, SEEK_SET);
	safeFread(block_bitmap, sizeof(block_bitmap), 1, fr);

	fseek(fr, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	safeFread(modified_inode_bitmap, sizeof(modified_inode_bitmap), 1, fr);

	fflush(fr);
	return true;
}



bool mkdir(Client& client, int PIAddr, char name[]) {	//目录创建函数(父目录权限:读写执行)
	//理论上client.Cur_Dir_Addr是系统分配的，应该是正确的
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		char ms[] = "Your filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	//查找父目录的空位置
	int bpos = -1;
	int dpos = -1;
	int empty_b = -1;
	inode parino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&parino, sizeof(parino), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(client.Cur_Group_Name, parino.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(client.Cur_User_Name, parino.i_uname) == 0) {
		role = 6;
	}
	if ((((parino.inode_mode >> role >> 1) & 1 == 0)) && (strcmp(client.Cur_User_Name, "root") != 0)) {
		char ms[] = "Permission denied, cannot create new directory.\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	for (int i = 0; i < 10; ++i) {
		int baddr = parino.i_dirBlock[i];
		if (baddr != -1) {//block已被使用 
			DirItem ditem[DirItem_Size];
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (ditem[j].inodeAddr == -1) {//未使用过的项
					bpos = i;
					dpos = j;
				}
				if (strcmp(ditem[j].itemName, name) == 0) {//判断：存在同名目录
					//inode temp;
					//safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
					//safeFread(&temp, sizeof(inode), 1, fr);
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
			char ms[] = "Current folder is full, no more files can be added into!\n";
			send(client.client_sock, ms, strlen(ms), 0);
			return false;
		}
		int baddr = balloc();
		if (baddr == -1)	return false;

		parino.i_dirBlock[empty_b] = baddr;
		parino.inode_file_size += BLOCK_SIZE;
		safeFseek(fw, client.Cur_Dir_Addr, SEEK_SET);
		safeFwrite(&parino, sizeof(parino), 1, fw);
		fflush(fw);

		//初始化新的DirItem
		DirItem ditem[DirItem_Size];
		for (int i = 0; i < DirItem_Size; ++i) {
			ditem[i].inodeAddr = -1;
			strcpy(ditem[i].itemName, "");
		}
		safeFseek(fw, baddr, SEEK_SET);
		safeFwrite(ditem, sizeof(ditem), 1, fr);

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
	safeFseek(fw, PIAddr, SEEK_SET);
	safeFwrite(&parino, sizeof(parino), 1, fw);


	DirItem paritem[DirItem_Size];
	safeFseek(fr, parino.i_dirBlock[bpos], SEEK_SET);
	safeFread(paritem, sizeof(paritem), 1, fr);
	fflush(fr);
	strcpy(paritem[dpos].itemName, name);
	paritem[dpos].inodeAddr = chiiaddr;
	safeFseek(fw, parino.i_dirBlock[bpos], SEEK_SET);
	safeFwrite(paritem, sizeof(paritem), 1, fw);

	//子节点更新
	inode chiino;
	chiino.inode_id = chiiaddr;
	chiino.inode_mode = MODE_DIR | DIR_DEF_PERMISSION;
	chiino.inode_file_count = 2; //"."和".."
	strcpy(chiino.i_uname, client.Cur_User_Name);
	strcpy(chiino.i_gname, client.Cur_Group_Name);
	chiino.inode_file_size = 1 * BLOCK_SIZE;
	time(&chiino.inode_change_time);
	time(&chiino.dir_change_time);
	time(&chiino.file_modified_time);
	memset(chiino.i_dirBlock, -1, sizeof(chiino.i_dirBlock));
	chiino.i_indirect_1 = -1;
	chiino.i_indirect_2 = -1;
	chiino.i_dirBlock[0] = chibaddr;
	safeFseek(fw, chiiaddr, SEEK_SET);
	safeFwrite(&chiino, sizeof(inode), 1, fw);

	DirItem chiitem[DirItem_Size];
	for (int i = 0; i < DirItem_Size; ++i) {
		chiitem[i].inodeAddr = -1;
		strcpy(chiitem[i].itemName, "");
	}
	chiitem[0].inodeAddr = chiiaddr;
	strcpy(chiitem[0].itemName, ".");
	chiitem[1].inodeAddr = client.Cur_Dir_Addr;
	strcpy(chiitem[1].itemName, "..");
	safeFseek(fw, chibaddr, SEEK_SET);
	safeFwrite(chiitem, sizeof(chiitem), 1, fw);

	fflush(fw);
	//DirItem ditem[DirItem_Size];
	//backup(count, 0);
	return true;
}

bool mkfile(Client& client, int PIAddr, char name[],char buf[]) {	//文件创建函数
	//理论上client.Cur_Dir_Addr是系统分配的，应该是正确的
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		char ms[] = "Your filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//查找父目录的空位置
	int bpos = -1;
	int dpos = -1;
	int empty_b = -1;
	inode parino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&parino, sizeof(parino), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(client.Cur_Group_Name, parino.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(client.Cur_User_Name, parino.i_uname) == 0) {
		role = 6;
	}
	if ((((parino.inode_mode >> role >> 1) & 1 == 0)) || (strcmp(client.Cur_User_Name, "root") != 0)) {
		char ms[] = "Permission denied (Not Previldged), cannot create new directory.\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	
	for (int i = 0; i < 10; ++i) {
		int baddr = parino.i_dirBlock[i];
		if (baddr != -1) {//block已被使用 
			DirItem ditem[DirItem_Size];
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (ditem[j].inodeAddr == -1) {//未使用过的项
					bpos = i;
					dpos = j;
				}
				if (strcmp(ditem[j].itemName, name) == 0) {//判断：存在同名目录
					char ms[] = "Directory already exists!\n";
					send(client.client_sock, ms, strlen(ms), 0);
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
			char ms[] = "Current folder is full, no more files can be added into!\n";
			send(client.client_sock, ms, strlen(ms), 0);
			return false;
		}
		int baddr = balloc();
		if (baddr == -1)	return false;

		parino.i_dirBlock[empty_b] = baddr;
		parino.inode_file_size += BLOCK_SIZE;
		safeFseek(fw, client.Cur_Dir_Addr, SEEK_SET);
		safeFwrite(&parino, sizeof(parino), 1, fw);
		fflush(fw);

		DirItem ditem[DirItem_Size];
		for (int i = 0; i < DirItem_Size; ++i) {
			ditem[i].inodeAddr = -1;
			strcpy(ditem[i].itemName, "");
		}
		safeFseek(fw, baddr, SEEK_SET);
		safeFwrite(ditem, sizeof(ditem), 1, fr);

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
	safeFseek(fw, PIAddr, SEEK_SET);
	safeFwrite(&parino, sizeof(parino), 1, fw);
	time(&parino.file_modified_time);

	DirItem paritem[DirItem_Size];
	safeFseek(fr, parino.i_dirBlock[bpos], SEEK_SET);
	safeFread(paritem, sizeof(paritem), 1, fr);
	fflush(fr);
	strcpy(paritem[dpos].itemName, name);
	paritem[dpos].inodeAddr = chiiaddr;
	safeFseek(fw, parino.i_dirBlock[bpos], SEEK_SET);
	safeFwrite(paritem, sizeof(paritem), 1, fw);

	inode chiino;
	chiino.inode_id = chiiaddr;
	chiino.inode_mode = MODE_FILE | FILE_DEF_PERMISSION;
	chiino.inode_file_count = 1; 
	strcpy(chiino.i_uname, client.Cur_User_Name);
	strcpy(chiino.i_gname, client.Cur_Group_Name);
	time(&chiino.inode_change_time);
	time(&chiino.dir_change_time);
	time(&chiino.file_modified_time);
	memset(chiino.i_dirBlock, -1, sizeof(chiino.i_dirBlock));
	chiino.i_indirect_1 = -1;
	chiino.i_indirect_2 = -1;
	chiino.i_dirBlock[0] = chibaddr;
	chiino.inode_file_size =0;
	safeFseek(fw, chiiaddr, SEEK_SET);
	safeFwrite(&chiino, sizeof(inode), 1, fw);

	writefile(chiino, chiiaddr, buf);//将buf信息写入(新开）
	
	fflush(fw);
	return true;
}
bool rm(Client& client, int PIAddr, char name[], int type) {	//删除文件or文件夹
	//文件和目录不允许重名
	inode ino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&ino, sizeof(ino), 1, fr);

	for (int i = 0; i < 10; ++i) {
		if (ino.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			safeFseek(fr, ino.i_dirBlock[i], SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0)
				{ //找到同名
					if (type == 1)
					{//1:目录
						if (recursive_rmdir(client, ditem[j].inodeAddr, name)) {	//成功删除
							//printf("已经成功删除目录及其子文件!\n");
							char ms[] = "Directory and all its inner files have been removed successfully!\n";
							send(client.client_sock, ms, strlen(ms), 0);
							strcpy(ditem[j].itemName, "");
							ditem[j].inodeAddr = -1;
							safeFseek(fw, ino.i_dirBlock[i], SEEK_SET);
							safeFwrite(ditem, sizeof(ditem), 1, fw);
						}
						else {
							char ms[] = "Failed!\n";
							send(client.client_sock, ms, strlen(ms), 0);
							return false;
						}
					}
					if (type == 0) {	//0:文件
						if (recursive_rmfile(client, ditem[j].inodeAddr, name)) {	//成功删除
							//printf("已经成功删除该文件!\n")
							char ms[] = "Successfully removed the file!\n";
							send(client.client_sock, ms, strlen(ms), 0);
							strcpy(ditem[j].itemName, "");
							ditem[j].inodeAddr = -1;

							safeFseek(fw, ino.i_dirBlock[i], SEEK_SET);
							safeFwrite(ditem, sizeof(ditem), 1, fw);
						}
						else {
							char ms[] = "Failed!\n";
							send(client.client_sock, ms, strlen(ms), 0);
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
	time(&ino.file_modified_time);
	safeFseek(fw, PIAddr, SEEK_SET);
	safeFwrite(&ino, sizeof(ino), 1, fw);
	return true;
}
bool recursive_rmdir(Client& client, int CHIAddr, char name[]) {//删除当前目录(父亲block和inode处的记录没删，不能直接用！！）
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		char ms[] = "Your filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if ((strcmp(name, ".") == 0) || strcmp(name, "..") == 0 ){
		//printf("文件无法删除\n");
		char ms[] = "Cannot delete the file!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//判断权限
	inode ino;
	safeFseek(fr, CHIAddr, SEEK_SET);
	safeFread(&ino, sizeof(inode), 1, fr);

	int mode = 0;//other
	if (strcmp(client.Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(client.Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 1) & 1) == 0) || (strcmp(client.Cur_User_Name, "root") != 0)) {//是否可写：2
		//printf("没有权限删除该文件夹\n");
		char ms[] = "No permission to delete this folder!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//删除其子文件夹
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		if (ino.i_dirBlock[i] != -1) {//被使用过
			safeFseek(fr, ino.i_dirBlock[i], SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				inode chiino;
				if (strcmp(ditem[j].itemName, ".") == 0 || strcmp(ditem[j].itemName, "..") == 0) {
					ditem[j].inodeAddr = -1;
					strcpy(ditem[j].itemName, "");
					continue;
				}
				if (strlen(ditem[j].itemName) != 0) {
					safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
					safeFread(&chiino, sizeof(inode), 1, fr);
					if ((chiino.inode_mode >> 9) & 1 == 1) {	//目录
						recursive_rmdir(client, ditem[j].inodeAddr, ditem[j].itemName);
					}
					else {										//文件
						recursive_rmfile(client, ditem[j].inodeAddr, ditem[j].itemName);
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
bool recursive_rmfile(Client& client, int CHIAddr, char name[]) {	//删除当前文件（父亲inode和block处的节点没删，不能直接用）
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		char ms[] = "Filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//判断权限
	inode ino;
	safeFseek(fr, CHIAddr, SEEK_SET);
	safeFread(&ino, sizeof(inode), 1, fr);

	int mode = 0;//other
	if (strcmp(client.Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(client.Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 1) & 1) == 0) || (strcmp(client.Cur_User_Name, "root") != 0)) {//是否可写：2
		char ms[] = "No permission to delete this file!\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
bool cat(Client& client, int PIAddr, char name[]) {	//查看文件内容
	inode parino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&parino, sizeof(parino), 1, fr);

	for (int i = 0; i < 10; ++i) {
		if (parino.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			safeFseek(fr, parino.i_dirBlock[i], SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) {	//同名
					inode chiino;
					safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
					safeFread(&chiino, sizeof(chiino), 1, fr);
					if(((chiino.inode_mode>>9)&1)==0){//文件
						//读文件内容
						for (int k = 0; k < 10;++k) {
							if (chiino.i_dirBlock[k] != -1) {
								char content[BLOCK_SIZE];
								safeFseek(fr, chiino.i_dirBlock[k], SEEK_SET);
								safeFread(content, sizeof(content), 1, fr);
								//printf("%s\n", content);
								char sendbuff[strlen(content) + 1];
								strcpy(sendbuff, content);
								sendbuff[strlen(content)] = '\n';
								send(client.client_sock, sendbuff, strlen(sendbuff), 0);
							}	
						}
					}
					return true;
				}
			}
		}
	}

	//printf("未找到该文件\n");
	char ms[] = "File not found!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	return false;
	
}
bool echo(Client& client, int PIAddr, char name[], int type, char* buf) {	//文件新增or重写or补全
	if(buf[0]=='"') buf += 1;
	if(buf[strlen(buf) - 1]=='"')  buf[strlen(buf) - 1] = '\0';
	if (type == 0) {
		if (mkfile(client, PIAddr, name, buf)) {
			return true;
		}
	}
	inode parino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&parino, sizeof(parino), 1, fr);
	for (int i = 0; i < 10; ++i) {
		if (parino.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			safeFseek(fr, parino.i_dirBlock[i], SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) {	//同名
					inode chiino;
					safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
					safeFread(&chiino, sizeof(chiino), 1, fr);
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
							if (addfile(client, chiino, ditem[j].inodeAddr, buf)) {
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

		safeFseek(fw, baddr, SEEK_SET);
		safeFwrite(temp_file, BLOCK_SIZE, 1, fw);

		fileinode.i_dirBlock[i] = baddr;
	}
	fileinode.inode_file_size = strlen(buf);
	time(&fileinode.inode_change_time);
	time(&fileinode.file_modified_time);
	safeFseek(fw, iaddr, SEEK_SET);
	safeFwrite(&fileinode, sizeof(fileinode), 1, fw);
	return true;
}

bool addfile(Client& client, inode fileinode, int iaddr, char buf[]) { //文件续写ok
	//前提：假设是按照block顺序存储
	if ((fileinode.inode_file_size + strlen(buf)) > 10 * BLOCK_SIZE) {
		//printf("文件内存不足，无法继续添加内容\n");
		char ms[] = "Running out file storage, cannot add more content!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	fileinode.inode_file_size += strlen(buf);
	time(&fileinode.inode_change_time);
	time(&fileinode.file_modified_time);
	safeFseek(fw, iaddr, SEEK_SET);
	safeFwrite(&fileinode, sizeof(inode), 1, fw);

	//写入文件
	int start = fileinode.inode_file_size / BLOCK_SIZE; //使用到第几块（考虑block[0])
	char temp[BLOCK_SIZE];
	for (int i = start; i < 10; ++i) {
		if (fileinode.i_dirBlock[i] != -1) {	//正在使用块(补全）
			safeFseek(fr, fileinode.i_dirBlock[i], SEEK_SET);
			safeFread(temp, BLOCK_SIZE, 1, fr);
			fflush(fr);
			int offset = BLOCK_SIZE - strlen(temp);
			strncat(temp, buf, offset);
			safeFseek(fw, fileinode.i_dirBlock[i], SEEK_SET);
			safeFwrite(temp, BLOCK_SIZE, 1, fw);
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
			safeFseek(fw, fileinode.i_dirBlock[i], SEEK_SET);
			safeFwrite(buf, BLOCK_SIZE, 1, fw);
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

bool cd(Client& client, int PIAddr, char name[]) {//切换目录
	inode pinode;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&pinode, sizeof(inode), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(client.Cur_Group_Name, pinode.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(client.Cur_User_Name, pinode.i_uname) == 0) {
		role = 6;
	}

	for (int i = 0; i < 10; ++i) {
		if (pinode.i_dirBlock[i] != -1) {
			DirItem ditem[DirItem_Size];
			safeFseek(fr, pinode.i_dirBlock[i], SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) { //找到同名
					if (strcmp(name, ".") == 0) {
						return true;
					}
					if (strcmp(name, "..") == 0) {
						if (strcmp(client.Cur_Dir_Name, "/") ==0){
							return true;
						}
						//char* p = strrchr(client.Cur_Dir_Addr, '/'); 跑不了啊
						char* p = client.Cur_Dir_Name+strlen(client.Cur_Dir_Name);
						while ((*p) != '/')p--;
						*p = '\0'; //打断它
						client.Cur_Dir_Addr = ditem[j].inodeAddr;
						return true;
					}
					inode chiino;
					safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
					safeFread(&chiino, sizeof(inode), 1, fr);
					fflush(fr);

					if ((((chiino.inode_mode >> role) & 1) == 1) ||(strcmp(client.Cur_Group_Name,"root")==0)){	//是否有执行权限
						if (strcmp(client.Cur_Dir_Name, "/") != 0) {
							strcat(client.Cur_Dir_Name, "/");
						}
						strcat(client.Cur_Dir_Name, name);
						client.Cur_Dir_Addr = ditem[j].inodeAddr;
						return true;
					}
				}
			}
		}
	}
	//printf("该文件夹不存在，无法进入\n");
	char mes[] = ("Folder not exists! Cannot access.\n");
	send(client.client_sock, mes, strlen(mes), 0);
	return false;
}
void gotoRoot(Client& client) { //ok
	client.Cur_Dir_Addr= Root_Dir_Addr;
	strcpy(client.Cur_Dir_Name , "/");
}

void ls(Client& client, char str[]) {//显示当前目录所有文件 ok
	inode ino;
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&ino, sizeof(inode), 1, fr);
	fflush(fr);
	//printf("%s\n", ino);
	
	//查看权限
	int mode = 0;//other
	if (strcmp(client.Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(client.Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 2) & 1 )== 0) &&(strcmp(client.Cur_User_Name, "root") != 0)) {//是否可读：4
		//printf("没有权限查看该文件夹\n");
		char mes[] = "You have no access to this folder!\n";
		send(client.client_sock, mes, strlen(mes), 0);
		return;
	}
	
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		if (ino.i_dirBlock[i] != -1) {//被使用过
			safeFseek(fr, ino.i_dirBlock[i], SEEK_SET);
			safeFread(ditem, sizeof(ditem), 1, fr);
			if (strcmp(str, "-l") == 0) 
			{
				//取出目录项的inode
				for (int j = 0; j < DirItem_Size; j++)
				{
					char to_send[BUF_SIZE];
					memset(to_send, '\0', BUF_SIZE);
					int ptr = 0;
					inode tmp;
					safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
					safeFread(&tmp, sizeof(inode), 1, fr);
					fflush(fr);

					if (strcmp(ditem[j].itemName, "") == 0|| (strcmp(ditem[j].itemName, ".") == 0) || (strcmp(ditem[j].itemName, "..") == 0)) {
						continue;
					}

					if (((tmp.inode_mode >> 9) & 1) == 1) {
						//printf("d");
						to_send[ptr++] = 'd';
					}
					else {
						//printf("-");
						to_send[ptr++] = '-';
					}
					
					//权限
					int count = 8;
					while (count >= 0) {
						if (((tmp.inode_mode >> count) & 1) == 1) {
							int mod = count % 3;
							switch (mod) {
							case 0:
								//printf("x");
								to_send[ptr++] = 'x';
								break;
							case 1:
								//printf("w");
								to_send[ptr++] = 'w';
								break;
							case 2:
								//printf("r");
								to_send[ptr++] = 'r';
								break;	
							}
						}
						else {
							//printf("-");
							to_send[ptr++] = '-';
						}
						count--;
					}
					//printf("\t");
					to_send[ptr++] = '\t';

					//printf("%s\t", tmp.i_uname);
					char new_buff1[1024];
					sprintf(new_buff1,"%s\t", tmp.i_uname);

					//printf("%s\t", tmp.i_gname);
					char new_buff2[1024];
					sprintf(new_buff2, "%s\t", tmp.i_gname);

					//printf("%s\t", tmp.inode_file_size);
					/*char* new_buff3;
					sprintf(new_buff3, "%d\t", tmp.inode_file_size);*/

					//printf("%s\t", ctime(&tmp.file_modified_time));
					char time_str[26];
					strcpy(time_str, ctime(&tmp.file_modified_time));
					char new_buff4[30];
					sprintf(new_buff4, "%s\t", time_str);

					//printf("%s\t", ditem[j].itemName);
					char new_buff5[1024];
					sprintf(new_buff5, "%s\n", ditem[j].itemName);

					//printf("\n");
					send(client.client_sock, to_send, strlen(to_send), 0);
					send(client.client_sock, new_buff1, strlen(new_buff1), 0);
					send(client.client_sock, new_buff2, strlen(new_buff2), 0);
					//send(client.client_sock, new_buff3, strlen(new_buff3), 0);
					send(client.client_sock, new_buff4, strlen(new_buff4), 0);
;					send(client.client_sock, new_buff5, strlen(new_buff5), 0);

				}
			}
			else {
					for (int j = 0; j < DirItem_Size; ++j) {
						if (strlen(ditem[j].itemName) != 0) 
						{
							char tosend[BUF_SIZE]; memset(tosend, '\0', BUF_SIZE);
							if ((strcmp(ditem[j].itemName, ".") == 0) || (strcmp(ditem[j].itemName, "..") == 0))
								continue;
							sprintf(tosend, "%s\n", ditem[j].itemName);
							send(client.client_sock, tosend, strlen(tosend), 0);
						}
					}
			}
		}
	}
	return;
}

//****工具函数****

std::mutex fileMutex;
// 自定义函数，对文件指针的操作进行加锁
void safeFseek(FILE* file, long offset, int origin) {
	std::lock_guard<std::mutex> lock(fileMutex);
	fseek(file, offset, origin);
}

size_t safeFread(void* ptr, size_t size, size_t count, FILE* file) {
    std::lock_guard<std::mutex> lock(fileMutex);
    return fread(ptr, size, count, file);
}

// 自定义函数，对文件写入进行加锁
size_t safeFwrite(const void* ptr, size_t size, size_t count, FILE* file) {
	std::lock_guard<std::mutex> lock(fileMutex);
	return fwrite(ptr, size, count, file);
}

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
	safeFseek(fw, Superblock_Start_Addr, SEEK_SET);
	safeFwrite(superblock, sizeof(superblock), 1, fw);
	safeFseek(fw, InodeBitmap_Start_Addr, SEEK_SET);
	safeFwrite(inode_bitmap, sizeof(inode_bitmap), 1, fw);
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
	safeFseek(fw, InodeBitmap_Start_Addr, SEEK_SET);
	safeFwrite(inode_bitmap, sizeof(inode_bitmap), 1, fw);
	inode ino;
	safeFseek(fw, iaddr, SEEK_SET);
	safeFwrite(&ino, sizeof(inode), 1, fw);
	superblock->s_free_INODE_NUM -= 1;
	safeFseek(fw, Superblock_Start_Addr, SEEK_SET);
	safeFwrite(superblock, sizeof(superblock), 1, fw);
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
	safeFseek(fw, Superblock_Start_Addr, SEEK_SET);
	safeFwrite(superblock, sizeof(superblock), 1, fw);
	safeFseek(fw, BlockBitmap_Start_Addr, SEEK_SET);
	safeFwrite(block_bitmap, sizeof(block_bitmap), 1, fw);
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
	safeFseek(fw, BlockBitmap_Start_Addr, SEEK_SET);
	safeFwrite(block_bitmap, sizeof(block_bitmap), 1, fw);
	superblock->s_free_BLOCK_NUM -= 1;
	safeFseek(fw, Superblock_Start_Addr, SEEK_SET);
	safeFwrite(superblock, sizeof(superblock), 1, fw);
}

//****用户&用户组函数****
void inUsername(Client& client, char* username)	//输入用户名
{
	char tosend[] = "username: ";
	auto i = send(client.client_sock, tosend, strlen(tosend), 0);
	memset(client.buffer, '\0', sizeof(client.buffer));
	i = recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(username, client.buffer);	//用户名
}
 
void inPasswd(Client& client, char* passwd)	//输入密码
{
	char tosend[] = "password: ";
	auto i = send(client.client_sock, tosend, strlen(tosend), 0);
	memset(client.buffer, '\0', sizeof(client.buffer));
	i = recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(passwd, client.buffer);
}

void ingroup(Client& client, char* group) 
{
	char tosend[] = "group (root;teacher;student): ";
	auto i = send(client.client_sock, tosend, strlen(tosend), 0);
	memset(client.buffer, '\0', sizeof(client.buffer));
	i = recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(group, client.buffer);
}

bool login(Client& client)	//登陆界面
{	
	//DirItem ditem[DirItem_Size];
	//safeFseek(fr,143872, SEEK_SET);
	//safeFread(ditem, sizeof(ditem), 1, fr);

	char username[100] = { 0 };
	char passwd[100] = { 0 };
	memset(username, '\0', sizeof(username));
	memset(passwd, '\0', sizeof(passwd));
	inUsername(client, username);	//输入用户名
	inPasswd(client, passwd);		//输入用户密码

	if (check(client, username, passwd))
	{
		//核对用户名和密码
		client.islogin = true;
		isLogin = true;
		bool flag = false;
		for (size_t i = 0; i < allClients.size(); i++)
			if (strcmp(allClients[i].Cur_User_Name, client.Cur_User_Name) == 0)
			{
				flag = true; // 如果找到了
				allClients[i] = client;
				client.ptr = i;
				break;
			}

		if (!flag) // 如果没有找到
		{
			allClients.emplace_back(client);
			size_t sub_ptr = allClients.size() - 1;
			client.ptr = sub_ptr;
		}
		return true;
	}
	else {
		isLogin = false;
		return false;
	}
}

bool logout(Client& client) {	//用户注销
	gotoRoot(client);
	strcmp(client.Cur_User_Name, "");
	strcmp(client.Cur_Group_Name, "");
	strcmp(client.Cur_User_Dir_Name, "");
	//isLogin = false; // 多用户时这个判定逻辑失效
	client.islogin = false;
	isLogin = ever_logging(); // 构造函数，只要有用户还处于登陆状态返回 true
	//printf("账户注销成功！\n");
	char ms[] = "Successfully logged out our system!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	close(client.client_sock);
	return true;
	//pause
}
bool useradd(Client& client, char username[], char passwd[], char group[]) {	//用户注册
	//权限判断
	if (strcmp(client.Cur_User_Name, "root") != 0) {
		char mess[] = "Permission denied! You have no previlidge to add users!\n";
		send(client.client_sock, mess, strlen(mess), 0);
		//printf("权限不足，无法添加用户！\n");
		return false;
	}
	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	strcpy(pro_cur_user_name, client.Cur_User_Name);
	strcpy(pro_cur_group_name, client.Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	
	//创建用户目录

	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "home");
	mkdir(client, client.Cur_Dir_Addr, username);

	//更改文件夹所有者
	chown(client, client.Cur_Dir_Addr, username, username, group);

	//获取etc三文件
	inode etcino,shadowino,passwdino,groupino;
	int shadowiddr, passwdiddr,groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "passwd") == 0) {
					passwdiddr = ditem[j].inodeAddr;
					safeFseek(fr, passwdiddr, SEEK_SET);
					safeFread(&passwdino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					safeFseek(fr, shadowiddr, SEEK_SET);
					safeFread(&shadowino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					safeFseek(fr,groupiddr, SEEK_SET);
					safeFread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//读取三文件内容并修改三文件
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	char a[10];
	char *gid= is_group(client, group, a);
	if (strcmp(gid,"-1")==0) {
		//printf("用户组别不正确，请重新输入");
		char ms[] = "Invalid group entered, please try again!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	int g = atoi(gid);

	//passwd
	memset(buf, '\0', sizeof(buf));
	for (int i = 0; i < 10; ++i) {
		if (passwdino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	//buf[strlen(buf)] = '\0'; (strcat可能会自动添加？）
	if (strstr(buf, username)!= NULL) {
		//printf("该用户名已存在\n");
		char mes[] = "Username already exists!\n";
		send(client.client_sock, mes, strlen(mes), 0);
		return false;
	}
	sprintf(buf + strlen(buf), "%s:%d:%d\n", username, nextUID++, g);
	passwdino.inode_file_size = strlen(buf);
	writefile(passwdino, passwdiddr, buf);

	char t[BLOCK_SIZE];
	safeFseek(fr, passwdino.i_dirBlock[0], SEEK_SET);
	safeFread(t, BLOCK_SIZE, 1, fr);//不知道能否成功
	fflush(fr);

	//shadow
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	sprintf(buf + strlen(buf), "%s:%s\n", username, passwd);
	shadowino.inode_file_size = strlen(buf);
	writefile(shadowino, shadowiddr, buf);

	safeFseek(fr, shadowino.i_dirBlock[0], SEEK_SET);
	safeFread(t, BLOCK_SIZE, 1, fr);
	fflush(fr);

	//group(root:0:XX,XX)
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (groupino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	//拼接状增加
	if (g != (nextGID - 1)) {
		char* p = strstr(buf, gid);
		while ((*p) != '\n') {
			p++;
		}
		p++;
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
	else {
		if (buf[strlen(buf) - 2] == ':') {
			sprintf(buf + strlen(buf) - 1, "%s\n", username);
		}
		else {
			sprintf(buf + strlen(buf) - 1, ",%s\n", username);
		}
	}
	groupino.inode_file_size = strlen(buf);
	writefile(groupino, groupiddr, buf);

	sys.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(sys.Cur_Dir_Name, pro_cur_dir_name);
	return true;
}
bool userdel(Client& client, char username[]) {	//用户删除
	if (strcmp(client.Cur_User_Name, "root") != 0) {
		//printf("权限不足，无法删除用户\n");
		char ms[] = "Permission denied! Cannot remove the user!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if (strcmp(username, "root") == 0) {
		//printf("无法删除管理员\n");
		char ms[] = "Cannot remove admin!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	//char pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	//strcpy(pro_cur_user_name, client.Cur_User_Name);
	//strcpy(pro_cur_group_name, client.Cur_Group_Name);
	//strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	//strcpy(client.Cur_User_Name, username);
	//strcpy(client.Cur_Group_Name, "");

	//获取etc三文件
	inode etcino, shadowino, passwdino, groupino;
	int shadowiddr, passwdiddr, groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "passwd") == 0) {
					passwdiddr = ditem[j].inodeAddr;
					safeFseek(fr, passwdiddr, SEEK_SET);
					safeFread(&passwdino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					safeFseek(fr, shadowiddr, SEEK_SET);
					safeFread(&shadowino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					safeFseek(fr, groupiddr, SEEK_SET);
					safeFread(&groupino, sizeof(inode), 1, fr);
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
			safeFseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	//buf[strlen(buf)] = '\0'; (strcat可能会自动添加？）
	char* p = strstr(buf, username);
	if (strstr(buf, username) == NULL) {
		//printf("该用户名不存在，无法删除\n");
		char ms[] = "Username does not exist, cannot remove it!\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
	nextUID--;

	//shadow
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
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
			safeFseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
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

	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "home");
	rm(client, client.Cur_Dir_Addr, username,1);	//找到父目录home即可进入

	//恢复现场
	if ((p=strstr(pro_cur_dir_name, username)) == NULL) { //原路径不包含删除文件夹
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
	}
	else {	//包含删除文件夹
		*(--p) = '\0';
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
	}
	//printf("用户删除成功!\n");
	char ms[] = "User deletion succeeded!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	return true;
}
bool check(Client& client, char username[], char passwd[]) {//核验身份登录&设置 ok
	//获取三文件
	inode etcino, shadowino, passwdino, groupino;
	int shadowiddr, passwdiddr, groupiddr;
	gotoRoot(sys);
	cd(sys, sys.Cur_Dir_Addr, "etc");
	safeFseek(fr, sys.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "passwd") == 0) {
					passwdiddr = ditem[j].inodeAddr;
					safeFseek(fr, passwdiddr, SEEK_SET);
					safeFread(&passwdino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					safeFseek(fr, shadowiddr, SEEK_SET);
					safeFread(&shadowino, sizeof(inode), 1, fr);
				}
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					safeFseek(fr, groupiddr, SEEK_SET);
					safeFread(&groupino, sizeof(inode), 1, fr);
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
	memset(buf, '\0', sizeof(buf));
	memset(temp, '\0', sizeof(temp));
	memset(checkpw, '\0', sizeof(checkpw));
	memset(group, '\0', sizeof(group));


	//shadow
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcpy(buf, temp);
		}
	}
    printf("username is: %s, passwd is %s\n", username, passwd);
	char* p = strstr(buf, username);
	if (p == NULL) {
		//printf("该用户不存在。请创建用户后重新登陆.\n");
		char ms[] = "Non-existent user! Please create your role before logging.\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
		//printf("密码不正确，请重新尝试！\n");
        //printf("Here!!! checkpw is %s, passwd is %s\n", checkpw, passwd);
		char ms[] = "Incorrect password! Please try again!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//passwd
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (passwdino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
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
		strcpy(client.Cur_Group_Name, "root");
	}
	else if (strcmp(group, "1") == 0) {
		strcpy(client.Cur_Group_Name, "teacher");
	}
	else if (strcmp(group, "2") == 0) {
		strcpy(client.Cur_Group_Name, "student");
	}
	strcpy(client.Cur_User_Name,username);
	sprintf(client.Cur_User_Dir_Name, "/home/%s", username);
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "home");
	cd(client, client.Cur_Dir_Addr, username);

	return true;
}
bool chmod(Client& client, int PIAddr, char name[], char* pmode) {//修改文件or目录权限（假定文件和目录也不能重名）
	if (strlen(name) > FILENAME_MAX) {
		//printf("文件名称超过最大长度\n");
		char ms[] = "Your filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		//printf("该文件无法修改权限\n");
		char ms[] = "Cannot change the previlidge of current file!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	inode ino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&ino, sizeof(inode), 1, fr);
	fflush(fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		safeFseek(fr, ino.i_dirBlock[i], SEEK_SET);
		safeFread(ditem, sizeof(ditem), 1, fr);
		for (int j = 0; j < DirItem_Size; ++j) {
			if (strcmp(ditem[j].itemName, name) == 0) {//找到同名文件
				inode chiino;
				safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
				safeFread(&chiino, sizeof(inode), 1, fr);
				fflush(fr);
				//1:目录 0：文件
				//if (((chiino.inode_mode >> 9) & 1 )!= type) {	//未找到同一类型文件
				//	continue;
				//}
				//只有创建者和管理员可以更改权限
				if ((strcmp(chiino.i_uname, client.Cur_User_Name) == 0) || strcmp(client.Cur_User_Name, "root") == 0) {
					unsigned short i_mode = chiino.inode_mode;

					//修改权限
					long int  mode = 0;
					if ((pmode[0] >= '0') && (pmode[0] <= '9')) {	//pmode是数字
						char* endptr;
						mode = strtol(pmode, &endptr, 8);
						if (strlen(endptr)!=0) {
							//printf("请使用正确的八进制数字\n");
							char ms[] = "Please use correct octal number!\n";
							send(client.client_sock, ms, strlen(ms), 0);
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
								//printf("命令格式错误\n");
								char ms[] = "Incorrect command format!\n";
								send(client.client_sock, ms, strlen(ms), 0);
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
								//printf("命令格式错误\n");
								char ms[] = "Incorrect command format!\n";
								send(client.client_sock, ms, strlen(ms), 0);
								return false;
							}

							//u,g,o,a
							permit[strlen(permit) - 2] = '\0';
							for (int i = 0; i < strlen(permit); ++i) {
								char s_user = permit[i];
								if ((s_user != 'u') && (s_user != 'g') && (s_user != 'o') && (s_user != 'a')) {
									//printf("命令格式错误\n");
									char ms[] = "Incorrect command format!\n";
									send(client.client_sock, ms, strlen(ms), 0);
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
					safeFseek(fw, ditem[j].inodeAddr, SEEK_SET);
					safeFwrite(&chiino, sizeof(inode), 1, fw);
					fflush(fw);
					return true;
				}
				else {
					//printf("权限不足\n");
					char ms[] = "Permission denied!\n";
					send(client.client_sock, ms, strlen(ms), 0);
					return false;
				}
			}
		}
	}
	//printf("没有找到该文件，无法修改权限\n");
	char ms[] = "File not found! Aborted changes!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	return false;
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
//	cd(client.Cur_Dir_Addr, "etc");
//	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
//	safeFread(&etcino, sizeof(inode), 1, fr);
//	for (int i = 0; i < 10; ++i) {
//		DirItem ditem[DirItem_Size];
//		int baddr = etcino.i_dirBlock[i];
//		if (baddr != -1) {
//			safeFseek(fr, baddr, SEEK_SET);
//			safeFread(&ditem, BLOCK_SIZE, 1, fr);
//			for (int j = 0; j < DirItem_Size; ++j) {
//				if (strcmp(ditem[j].itemName, "passwd") == 0) {
//					passwdiddr = ditem[j].inodeAddr;
//					safeFseek(fr, passwdiddr, SEEK_SET);
//					safeFread(&passwdino, sizeof(inode), 1, fr);
//				}
//			}
//		}
//	}
//
//	memset(buf, '\0', sizeof(temp));
//	for (int i = 0; i < 10; ++i) {
//		if (passwdino.i_dirBlock[i] != -1) {
//			memset(temp, '\0', sizeof(temp));
//			safeFseek(fr, passwdino.i_dirBlock[i], SEEK_SET);
//			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
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

bool groupadd(Client& client, char* group) 
{
	//判断权限
	if (strcmp(client.Cur_User_Name, "root")!=0) {
		//printf("权限不足，无法增加用户组.\n");
		char ms[] = "Permission denied! You can not add new user group!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	strcpy(pro_cur_user_name, client.Cur_User_Name);
	strcpy(pro_cur_group_name, client.Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	//去到etc目录
	if (cd_func(client, client.Cur_Dir_Addr, "/etc") == false) {
		return false;
	}
	
	//获取group文件
	inode etcino,groupino;
	int groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					safeFseek(fr, groupiddr, SEEK_SET);
					safeFread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//读取三文件内容并修改三文件
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	//group(root:0:XX,XX)
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (groupino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	
	//判断group是否重复
	if (strstr(buf, group) != NULL) {
		//printf("组别已存在\n");
		char ms[] = "Group already exists!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}

	sprintf(buf + strlen(buf), "%s:%d:\n", group, nextGID++);
	if (writefile(groupino, groupiddr, buf) == false) {
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}

	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);

	return true;
}
bool groupdel(Client& client, char* group) {
	//判断权限
	if (strcmp(client.Cur_User_Name, "root") != 0) {
		//printf("权限不足，无法增加用户组.\n");
		char ms[] = "Permission denied! You can not create new user group!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if (strcmp(group, "root") == 0) {
		//printf("无法删除主用户群\n");
		char ms[] = "Can not delete 'root' user group!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	strcpy(pro_cur_user_name, client.Cur_User_Name);
	strcpy(pro_cur_group_name, client.Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	//去到etc目录
	if (cd_func(client, client.Cur_Dir_Addr, "/etc") == false) {
		return false;
	}

	//获取group文件
	inode etcino, groupino;
	int groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					safeFseek(fr, groupiddr, SEEK_SET);
					safeFread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//读取group
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	for (int i = 0; i < 10; ++i) {
		if (groupino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	char* names = strstr(buf, group);
	if (strlen(names) == 0) {
		//printf("该组别不存在\n");
		char ms[] = "This group does not exist!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	int flag = 0;
	*names = '\0';
	names++;
	while (true) {	//指向组内用户
		if (flag == 2) {
			break;
		}
		if ((*names) == ':') {
			flag++;
		}
		names++;
	}

	//删除组内用户
	while (true) {
		char name[20];
		memset(name, '\0', sizeof(name));
		char* p = strstr(names, ",");
		if (p==NULL) {
			strncpy(name, names,strlen(names)-1);
			userdel(client, name);
			break;
		}
		else {
			strncpy(name, names, p - names);
			names = p + 1;
			userdel(client, name);
		}
	}

	//更新group
	writefile(groupino, groupiddr, buf);
	nextGID--;
	
	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
	return true;
}
char* is_group(Client& client, char* group,char *gid) {
	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	strcpy(pro_cur_user_name, client.Cur_User_Name);
	strcpy(pro_cur_group_name, client.Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	//去到etc目录
	if (cd_func(client, client.Cur_Dir_Addr, "/etc") == false) {
		return "-1";
	}

	//获取group文件
	inode etcino, groupino;
	int groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "group") == 0) {
					groupiddr = ditem[j].inodeAddr;
					safeFseek(fr, groupiddr, SEEK_SET);
					safeFread(&groupino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//读取三文件内容并修改三文件
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	//group(root:0:XX,XX)
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (groupino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, groupino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}

	//判断group是否存在
	char* p = strstr(buf, group);
	memset(gid, '\0', sizeof(gid));
	int i = 0;
	if (p == NULL) {
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return "-1";
	}
	else {
		p = strstr(p, ":");
		p++;
		while ((*p) != ':') {
			gid[i++] = (*p);
			p++;
		}
	}

	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
	return gid;
}
bool chown(Client& client, int PIAddr,char* filename, char name[], char group[]) {//修改文件所属用户和用户组
	//判断
	if (strlen(filename) > FILENAME_MAX) {
		char ms[] = "Your filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
		//printf("该文件无法修改权限\n");
		char ms[] = "Unable to modify the permissions of this file!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	char gid[10];
	if (strcmp(is_group(client, group, gid), "-1") == 0) {
		//printf("组别不正确！请重新输入！\n");
		char ms[] = "Invalid group entered! Please try again!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	inode ino;
	safeFseek(fr, PIAddr, SEEK_SET);
	safeFread(&ino, sizeof(inode), 1, fr);
	fflush(fr);
	for (int i = 0; i < 10; ++i)
	{
		DirItem ditem[DirItem_Size];
		safeFseek(fr, ino.i_dirBlock[i], SEEK_SET);
		safeFread(ditem, sizeof(ditem), 1, fr);
		for (int j = 0; j < DirItem_Size; ++j) {
			if (strcmp(ditem[j].itemName, filename) == 0) {//找到同名文件
				inode chiino;
				safeFseek(fr, ditem[j].inodeAddr, SEEK_SET);
				safeFread(&chiino, sizeof(inode), 1, fr);
				fflush(fr);
				//1:目录 0：文件
				//if (((chiino.inode_mode >> 9) & 1 )!= type) {	//未找到同一类型文件
				//	continue;
				//}
				//只有创建者和管理员可以更改用户组or用户
				if ((strcmp(chiino.i_uname, client.Cur_User_Name) == 0) || strcmp(client.Cur_User_Name, "root") == 0) {
					if (strlen(name) != 0) { strcpy(chiino.i_uname, name); }
					if (strlen(group) != 0) { strcpy(chiino.i_gname, group); }
					safeFseek(fw, ditem[j].inodeAddr, SEEK_SET);
					safeFwrite(&chiino, sizeof(inode), 1, fw);
					fflush(fw);
					return true;
				}
				else {
					//printf("权限不足\n");
					char ms[] = "Permission Denied!\n";
					send(client.client_sock, ms, strlen(ms), 0);
					return false;
				}
			}
		}
	}
	//printf("没有找到该文件，无法修改权限\n");
	char ms[] = "File not found! Changes aborted!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	return false;
}
bool passwd(Client& client, char username[],char pwd[]) {

	//设定修改用户名称
	char uname[100];
	if (strlen(username) == 0) {
		strcpy(uname, client.Cur_User_Name);
	}
	else {
		strcpy(uname, username);
	}
	
	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	strcpy(pro_cur_user_name, client.Cur_User_Name);
	strcpy(pro_cur_group_name, client.Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	//获取etc三文件
	inode etcino, shadowino;
	int shadowiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	safeFseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	safeFread(&etcino, sizeof(inode), 1, fr);
	for (int i = 0; i < 10; ++i) {
		DirItem ditem[DirItem_Size];
		int baddr = etcino.i_dirBlock[i];
		if (baddr != -1) {
			safeFseek(fr, baddr, SEEK_SET);
			safeFread(&ditem, BLOCK_SIZE, 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, "shadow") == 0) {	//不判断是否为文件了
					shadowiddr = ditem[j].inodeAddr;
					safeFseek(fr, shadowiddr, SEEK_SET);
					safeFread(&shadowino, sizeof(inode), 1, fr);
				}
			}
		}
	}

	//shadow
	char buf[BLOCK_SIZE * 10]; //1char:1B
	char temp[BLOCK_SIZE];
	memset(buf, '\0', sizeof(temp));
	for (int i = 0; i < 10; ++i) {
		if (shadowino.i_dirBlock[i] != -1) {
			memset(temp, '\0', sizeof(temp));
			safeFseek(fr, shadowino.i_dirBlock[i], SEEK_SET);
			safeFread(&temp, BLOCK_SIZE, 1, fr);//不知道能否成功
			strcat(buf, temp);
		}
	}
	char* p = strstr(buf, uname);
	if (p == NULL) {
		//printf("该用户不存在\n");
		char ms[] = "User does not exist!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcmp(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	*p = '\0';
	while ((*p) != '\n') {
		p++;
	}
	p++;
	char content[BLOCK_SIZE * 10];
	memset(content, '\0', sizeof(content));
	strcpy(content, p);
	sprintf(buf + strlen(buf), "%s:%s\n", uname, pwd);
	strcat(buf, content);
	writefile(shadowino, shadowiddr, buf);

	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcmp(client.Cur_Dir_Name, pro_cur_dir_name);
	return true;
}
