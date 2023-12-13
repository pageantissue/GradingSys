#include"os.h"
#include"server.h"
#include<ctime>
#include<cstring>
#include<cstdio>
#include<mutex>
using namespace std;

Client sys;
std::vector<Client> allClients;
std::mutex workPrt;

void help(Client& client)
{
	char help[] = "help";
	send(client.client_sock, help, strlen(help), 0);
}

//****大类函数****
bool Format() { //ok
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
	mkdir(sys, sys.Cur_Dir_Addr, "home");
	cd(sys, sys.Cur_Dir_Addr, "home");
	mkdir(sys, sys.Cur_Dir_Addr, "root");
	
	gotoRoot(sys);
	mkdir(sys, sys.Cur_Dir_Addr, "etc");
	cd(sys, sys.Cur_Dir_Addr, "etc");

	char buf[1000] = { 0 };
	sprintf(buf, "root:%d:%d\n", nextUID++, nextGID++);//root:uid-0,gid-0
	mkfile(sys, sys.Cur_Dir_Addr, "passwd", buf);

	int pmode = 0400;//owner:可读
	sprintf(buf, "root:root\n");
	mkfile(sys, sys.Cur_Dir_Addr, "shadow", buf);
	chmod(sys, sys.Cur_Dir_Addr, "shadow", pmode,0);

	sprintf(buf, "root:%d:root\n", ROOT);
	sprintf(buf + strlen(buf), "teacher:%d:\n", TEACHER);
	sprintf(buf + strlen(buf), "student:%d:\n", STUDENT);
	mkfile(sys, sys.Cur_Dir_Addr, "group", buf);
	
	gotoRoot(sys);
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

bool mkdir(Client& client, int PIAddr, char name[]) {	//目录创建函数(父目录权限:写)(ok
	//理论上Cur_Dir_Addr是系统分配的，应该是正确的

	/*if (strlen(name) > FILE_NAME_MAX_SIZE) {
		printf("文件名称超过最大长度\n");
		return false;
	}*/
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
	fseek(fr, PIAddr, SEEK_SET);
	fread(&parino, sizeof(parino), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(client.Cur_Group_Name, parino.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(client.Cur_User_Name, parino.i_uname) == 0) {
		role = 6;
	}
	if ((((parino.inode_mode >> role >> 1) & 1 == 0) )&& (strcmp(client.Cur_User_Name, "root") != 0)) {
		//printf("权限不足，无法新建目录\n");
		char ms[] = "Permission denied, cannot create new directory.\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
					inode temp;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&temp, sizeof(inode), 1, fr);
					if (((temp.inode_mode >> 9) & 1) == 1) {//是目录
						//printf("该目录下已包含同名目录\n");
						char ms[] = "Directory already exists!\n";
						send(client.client_sock, ms, strlen(ms), 0);
						return false;
					}
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
			}
		}
		if (empty_b == -1) {
			//printf("该目录已满，无法添加更多文件");
			char ms[] = "Current folder is full, no more files can be added into!\n";
			send(client.client_sock, ms, strlen(ms), 0);
			return false;
		}
		int baddr = balloc();
		if (baddr == -1)	return false;

		parino.i_dirBlock[empty_b] = baddr;
		parino.inode_file_size += BLOCK_SIZE;
		fseek(fw, client.Cur_Dir_Addr, SEEK_SET);
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
	fseek(fw, chiiaddr, SEEK_SET);
	fwrite(&chiino, sizeof(inode), 1, fw);

	DirItem chiitem[DirItem_Size];
	for (int i = 0; i < DirItem_Size; ++i) {
		chiitem[i].inodeAddr = -1;
		strcpy(chiitem[i].itemName, "");
	}
	chiitem[0].inodeAddr = chiiaddr;
	strcpy(chiitem[0].itemName, ".");
	chiitem[1].inodeAddr = client.Cur_Dir_Addr;
	strcpy(chiitem[1].itemName, "..");
	fseek(fw, chibaddr, SEEK_SET);
	fwrite(chiitem, sizeof(chiitem), 1, fw);

	fflush(fw);
	DirItem ditem[DirItem_Size];
	return true;
}

bool mkfile(Client& client, int PIAddr, char name[],char buf[]) {	//文件创建函数
	//理论上Cur_Dir_Addr是系统分配的，应该是正确的
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
	fseek(fr, PIAddr, SEEK_SET);
	fread(&parino, sizeof(parino), 1, fr);

	//判断身份
	int role = 0;	//other 0
	if (strcmp(client.Cur_Group_Name, parino.i_gname) == 0) {
		role = 3;	//group 3
	}
	if (strcmp(client.Cur_User_Name, parino.i_uname) == 0) {
		role = 6;
	}
	if ((((parino.inode_mode >> role >> 1) & 1 == 0)) && (strcmp(client.Cur_User_Name, "root") != 0)) {
		//printf("权限不足，无法新建目录\n");
		char ms[] = "Permission denied, cannot create new directory.\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
					inode temp;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&temp, sizeof(inode), 1, fr);
					if (((temp.inode_mode >> 9) & 1) == 1) {//是目录
						//printf("该目录下已包含同名目录\n");
						char ms[] = "Directory already exists!\n";
						send(client.client_sock, ms, strlen(ms), 0);
						return false;
					}
				}
			}
		}
	}
	fflush(fr);

	if (bpos == -1 || dpos == -1) {	//block不足要新开
		for (int i = 0; i < 10; ++i) {
			if (parino.i_dirBlock[i] == -1) {
				empty_b = i;
			}
		}
		if (empty_b == -1) {
			//printf("该目录已满，无法添加更多文件");
			char ms[] = "Current folder is full, no more files can be added into!\n";
			send(client.client_sock, ms, strlen(ms), 0);
			return false;
		}
		int baddr = balloc();
		if (baddr == -1)	return false;

		parino.i_dirBlock[empty_b] = baddr;
		parino.inode_file_size += BLOCK_SIZE;
		fseek(fw, client.Cur_Dir_Addr, SEEK_SET);
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
	fseek(fw, chiiaddr, SEEK_SET);
	fwrite(&chiino, sizeof(inode), 1, fw);

	writefile(chiino, chiiaddr, buf);//将buf信息写入(新开）
	
	fflush(fw);
	return true;
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

		//char t[BLOCK_SIZE];
		//fseek(fr, baddr+7, SEEK_SET);
		//fread(t, BLOCK_SIZE, 1, fr);//不知道能否成功
		//fseek(fr, fileinode.i_dirBlock[0], SEEK_SET);
		//fread(t, BLOCK_SIZE, 1, fr);//不知道能否成功

		fileinode.i_dirBlock[i] = baddr;
	}
	fileinode.inode_file_size = strlen(buf);
	time(&fileinode.inode_change_time);
	time(&fileinode.file_modified_time);
	fseek(fw, iaddr, SEEK_SET);
	fwrite(&fileinode, sizeof(fileinode), 1, fw);

	//char t[BLOCK_SIZE];
	//fseek(fr, fileinode.i_dirBlock[0], SEEK_SET);
	//fread(t, BLOCK_SIZE, 1, fr);//不知道能否成功


	return true;
}
bool rmdir(Client& client, int CHIAddr, char name[]) {//删除当前目录
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		//printf("文件名称超过最大长度\n");
		char ms[] = "Filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if ((strcmp(name, ".") == 0) || strcmp(name, "..") == 0 ){
		//printf("文件无法删除\n");
		char ms[] = "File cannnot be deleted!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//判断权限
	inode ino;
	fseek(fr, CHIAddr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);

	int mode = 0;//other
	if (strcmp(client.Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(client.Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 1) & 1) == 0) && (strcmp(client.Cur_User_Name, "root") != 0)) {//是否可写：2
		//printf("没有权限删除该文件夹\n");
		char ms[] = "No permission to delete this directory!\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
						rmdir(client, ditem[j].inodeAddr, ditem[j].itemName);
					}
					else {										//文件
						rmfile(client, ditem[j].inodeAddr, ditem[j].itemName);
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
bool rmfile(Client& client, int CHIAddr, char name[]) {	//删除当前文件
	if (strlen(name) > FILE_NAME_MAX_SIZE) {
		//printf("文件名称超过最大长度\n");
		char ms[] = "Filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//判断权限
	inode ino;
	fseek(fr, CHIAddr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);

	int mode = 0;//other
	if (strcmp(client.Cur_Group_Name, ino.i_gname) == 0) {//group
		mode = 3;
	}
	if (strcmp(client.Cur_User_Name, ino.i_uname) == 0) {//owner
		mode = 6;
	}
	if ((((ino.inode_mode >> mode >> 1) & 1) == 0) && (strcmp(client.Cur_User_Name, "root") != 0)) {//是否可写：2
		//printf("没有权限删除该文件\n");
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
bool addfile(Client& client, inode fileinode, int iaddr, char buf[]){ //文件续写ok
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
bool cd(Client& client, int PIAddr, char name[]) {//切换目录(ok
	inode pinode;
	fseek(fr, PIAddr, SEEK_SET);
	fread(&pinode, sizeof(inode), 1, fr);

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
			fseek(fr, pinode.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			for (int j = 0; j < DirItem_Size; ++j) {
				if (strcmp(ditem[j].itemName, name) == 0) { //找到同名
					if (strcmp(name, ".") == 0) {
						return true;
					}
					if (strcmp(name, "..") == 0) {
						if (strcmp(client.Cur_Dir_Name, "/") ==0){
							return true;
						}
						//char* p = strrchr(Cur_Dir_Addr, '/'); 跑不了啊
						char* p = client.Cur_Dir_Name+strlen(client.Cur_Dir_Name);
						while ((*p) != '/')p--;
						*p = '\0'; //打断它
						client.Cur_Dir_Addr = ditem[j].inodeAddr;
						return true;
					}
					inode chiino;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fread(&chiino, sizeof(inode), 1, fr);
					fflush(fr);
					if (((chiino.inode_mode >> role) & 1) == 1) {	//是否有执行权限
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
	client.Cur_Dir_Addr = Root_Dir_Addr;
	strcpy(client.Cur_Dir_Name , "/");
}

char* send_init(char* to_send)
{
    size_t new_length = strlen(to_send) + 1 + 1;
    char* new_buff = (char*)malloc(new_length);
    strcpy(new_buff, to_send);
    new_buff[strlen(to_send)] = '\t';
    return new_buff;
}

void ls(Client& client, char str[]) {//显示当前目录所有文件 ok
	inode ino;
	fseek(fr, client.Cur_Dir_Addr, SEEK_SET);
	fread(&ino, sizeof(inode), 1, fr);
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
		if (ino.i_dirBlock[i] != -1)
		{//被使用过
			fseek(fr, ino.i_dirBlock[i], SEEK_SET);
			fread(ditem, sizeof(ditem), 1, fr);
			if (strcmp(str, "-l") == 0) 
			{
                printf("Here we entered ls -l\n");
				//取出目录项的inode
                printf("Ready\n");
				for(auto & j : ditem)
				{
                    char to_send[BUF_SIZE];
                    memset(to_send, '\0', BUF_SIZE);
                    int ptr = 0;
					inode tmp;
					fseek(fr, j.inodeAddr, SEEK_SET);
					fread(&tmp, sizeof(inode), 1, fr);
					fflush(fr);

					if (strcmp(j.itemName, "") == 0|| (strcmp(j.itemName, ".") == 0) || (strcmp(j.itemName, "..") == 0)) {
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
					while (count >= 0)
                    {
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
							default:
								//printf("-");
								to_send[ptr++] = '-';
							}
						}
						count--;
					}
					//printf("\t");
					to_send[ptr++] = '\t';

					//printf("%s\t", tmp.i_uname);
                    char* new_buff1 = send_init(tmp.i_uname);

					//printf("%s\t", tmp.i_gname);
                    char* new_buff2 = send_init(tmp.i_gname);

					//printf("%s\t", tmp.inode_file_size);
                    char* new_buff3;
                    sprintf(new_buff3, "%d",tmp.inode_file_size);

					//printf("%s\t", ctime(&tmp.file_modified_time));
                    char* new_buff4  = send_init(ctime(&tmp.file_modified_time));

					//printf("%s\t", ditem[j].itemName);
                    char* new_buff5 = send_init(j.itemName);

					//printf("\n");
					char newline[] = "\n";
                    send(client.client_sock, to_send, strlen(to_send), 0);
                    send(client.client_sock, new_buff1, strlen(new_buff1), 0);
                    send(client.client_sock, new_buff2, strlen(new_buff2), 0);
                    send(client.client_sock, new_buff3, strlen(new_buff3), 0);
                    send(client.client_sock, new_buff4, strlen(new_buff4), 0);
                    send(client.client_sock, new_buff5, strlen(new_buff5), 0);
					send(client.client_sock, newline, strlen(newline), 0);
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
void inUsername(Client& client, char* username)	//输入用户名
{
	char tosend[] = "username:";
	auto i = send(client.client_sock, tosend, strlen(tosend), 0);
	memset(client.buffer, '\0', sizeof(client.buffer));
	i = recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(username, client.buffer);	//用户名
}
 
void inPasswd(Client& client, char* passwd)	//输入密码
{
	char tosend[] = "password:";
	auto i = send(client.client_sock, tosend, strlen(tosend), 0);
	memset(client.buffer, '\0', sizeof(client.buffer));
	i = recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(passwd, client.buffer);
}

void ingroup(Client& client, char* group) {
	char tosend[] = "group (root;teacher;student): ";
	auto i = send(client.client_sock, tosend, strlen(tosend), 0);
	memset(client.buffer, '\0', sizeof(client.buffer));
	i = recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(group, client.buffer);
}

bool login(Client& client)	//登陆界面
{	
	//DirItem ditem[DirItem_Size];
	//fseek(fr,143872, SEEK_SET);
	//fread(ditem, sizeof(ditem), 1, fr);
	char username[100];
	char passwd[100];
	memset(username, '\0', sizeof(username));
	memset(passwd, '\0', sizeof(passwd));
	inUsername(client, username);	//输入用户名
	inPasswd(client, passwd);		//输入用户密码
	if (check(client, username, passwd)){  //核对用户名和密码
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
		client.islogin = false;
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

	//获取etc三文件
	inode etcino,shadowino,passwdino,groupino;
	int shadowiddr, passwdiddr,groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	fseek(fr, client.Cur_Dir_Addr, SEEK_SET);
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
		//printf("Invalid group entered, please try again!\n");
		char mes[] = "Invalid group entered, please try again!\n";
		send(client.client_sock, mes, strlen(mes), 0);
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
		char mes[] = "Username already exists!\n";
		send(client.client_sock, mes, strlen(mes), 0);
		//printf("该用户名已存在\n");
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

	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
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
		char ms[] = "Cannot remove admin!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		//printf("无法删除管理员\n");
		return false;
	}
	//保护现场并更改信息
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310], pro_cur_user_name[110], pro_cur_group_name[110], pro_cur_user_dir_name[310];
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	strcpy(pro_cur_user_name, client.Cur_User_Name);
	strcpy(pro_cur_group_name, client.Cur_Group_Name);
	strcpy(pro_cur_user_dir_name, client.Cur_User_Dir_Name);

	strcpy(client.Cur_User_Name, username);
	strcpy(client.Cur_Group_Name, "");

	//获取etc三文件
	inode etcino, shadowino, passwdino, groupino;
	int shadowiddr, passwdiddr, groupiddr;
	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "etc");
	fseek(fr, client.Cur_Dir_Addr, SEEK_SET);
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

	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "home");
	cd(client, client.Cur_Dir_Addr, username);
	rmdir(client, client.Cur_Dir_Addr, username);


	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);

	return true;
}
bool check(Client& client, char username[], char passwd[]) {//核验身份登录&设置 ok
	//获取三文件
	inode etcino, shadowino, passwdino, groupino;
	int shadowiddr, passwdiddr, groupiddr;
	gotoRoot(sys);
	cd(sys, sys.Cur_Dir_Addr, "etc");
	fseek(fr, sys.Cur_Dir_Addr, SEEK_SET);
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
    memset(checkpw, '\0', sizeof(checkpw));

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
        printf("checkpw = %s\nyou entered = %s", checkpw, passwd);
		char ms[] = "Incorrect password! Please try again!\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
bool chmod(Client& client, int PIAddr, char name[], int pmode,int type) {//修改文件or目录权限（假定文件和目录也不能重名）
	if (strlen(name) > FILENAME_MAX) {
		//printf("文件名称超过最大长度\n");
		char ms[] = "Your filename exceeds the max length supported!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}
	if (strcmp(name, ".") ==0|| strcmp(name, "..")==0) {
		//printf("该文件无法修改权限\n");
		char ms[] = "Cannot change the previlidge of this file!\n";
		send(client.client_sock, ms, strlen(ms), 0);
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
			if (strcmp(ditem[j].itemName, name)==0) {//找到同名文件
				inode chiino;
				fseek(fr, ditem[j].inodeAddr, SEEK_SET);
				fread(&chiino, sizeof(inode), 1, fr);
				fflush(fr);
				//1:目录 0：文件
				if (((chiino.inode_mode >> 9) & 1 )!= type) {	//未找到同一类型文件
					continue;
				}
				//只有创建者和管理员可以更改权限
				if ((strcmp(chiino.i_uname, client.Cur_User_Name) == 0) || strcmp(client.Cur_User_Name, "root")==0) {
					chiino.inode_mode = (chiino.inode_mode >> 9 << 9) | pmode;
					fseek(fr, ditem[j].inodeAddr, SEEK_SET);
					fwrite(&chiino, sizeof(inode), 1, fr);
					fflush(fw);
					return true;
				}
			}
		}
	}
	//printf("没有找到该文件，无法修改权限\n");
	char ms[] = "File not found! Changes aborted!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	return false;
}
void cmd(Client& client, int count)
{

	char com1[100];
	char com2[100];
	char com3[100];
	char cmd[BUF_SIZE] = "";
	strcpy(cmd, client.buffer);
	sscanf(cmd,"%s", com1);

	// Lock the file pointer before execute the cammand which refers to critical rescources
	lock_guard<std::mutex> lock(workPrt);

	if (strcmp(com1, "ls") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		ls(client, com2);
	}
	else if (strcmp(com1, "help") == 0) {
		help(client);
	}
	else if (strcmp(com1, "cd") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		cd(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "mkdir") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		mkdir(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "mkfile") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		char temp[100];
		memset(temp, '\0', strlen(temp));
		//printf("输入你需要的内容：\n");
		char mes[] = "Please enter your content: ...";
		send(client.client_sock, mes, strlen(mes), 0);
		recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
		mkfile(client, client.Cur_Dir_Addr, com2, client.buffer);
	}
	else if (strcmp(com1, "rmdir") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		rmdir(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "rmfile") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		rmfile(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "mkfile") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		mkfile(client, client.Cur_Dir_Addr, com2,"");
	}		//这个第三个参数是啥？不太懂
	else if (strcmp(com1, "useradd") == 0) {
		inUsername(client, com1);
		inPasswd(client, com2);
		ingroup(client, com3);
		useradd(client, com1, com2, com3);
	}
	else if (strcmp(com1, "userdel") == 0) {
		sscanf(cmd, "%s%s", com1, com2);
		userdel(client, com2);
	}
	else if(strcmp(com1,"logout")==0){
		logout(client);
	}
	else if (strcmp(com1, "format") == 0)
	{
		if (strcmp(client.Cur_User_Name, "root") != 0) {
			//cout << "您的权限不足" << endl;
			char ms[] = "You have no permission!\n";
			send(client.client_sock, ms, strlen(ms), 0);
		}
		logout(client);
	}
	else if (strcmp(com1, "exit") == 0) {
		//cout << "退出成绩管理系统，拜拜！" << endl;
		char ms[] = "Exitting our Grading System..... See you!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		exit(0);
	}
	// mutex would be automatically unlocked and released after the cmd function returned.
	return;                             
}

