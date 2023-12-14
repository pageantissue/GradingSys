#include"snapshot.h"
#include"os.h"
#include<cstdio>
#include<ctime>
#include<vector>
#include<string>
#include<dirent.h>
#include<unistd.h>

using namespace std;


bool fullBackup() {
	//�ҵ�֮ǰ��fullbackup�ļ�ɾ��
	DIR* dir;
	struct dirent* ent;
	if ((dir = opendir("./")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (strncmp(ent->d_name, "Full",4) == 0) {
				string path = "./" + (string)ent->d_name;
				if (unlink(path.c_str())) {
					printf("Delete %s\n", ent->d_name);
				}
			}
			else if (strncmp(ent->d_name, "Incre",5) == 0) {
				string path = "./" + (string)ent->d_name;
				if (unlink(path.c_str())) {
					printf("Delete %s\n", ent->d_name);
				}
			}
		}
	}

	//�����µ�fullbackup�ļ�
	time_t cur_time;
	time(&cur_time);
	//string bSysName;
	char backupSysName[256];
	memset(backupSysName, '\0', sizeof(backupSysName));
	strftime(backupSysName, sizeof(backupSysName), "FullBackupSys %Y.%m.%d %H-%M-%S.sys", localtime(&cur_time));
	printf("%s\n", backupSysName);
	if ((bfr = fopen(backupSysName, "rb")) == NULL) {
		bfw = fopen(backupSysName, "wb");
		if (bfw == NULL) {
			printf("BackupSys File error��");
			return false;
		}
		bfr = fopen(backupSysName, "rb");
		printf("Successfully open");
	}
	else {
		bfw = fopen(backupSysName, "rb+");
		if (bfw == NULL) {
			printf("BackupSys File error��");
			return false;
		}
	}
	
	//�ѱ��ݵĿռ��ʼ��
	char buffer[Disk_Size];
	char temp[BLOCK_SIZE];
	memset(buffer, '\0', sizeof(buffer));
	int c_Addr = 0;
	int num = Disk_Size / BLOCK_SIZE;
	for (int i = 0; i < num; i++) {
		
		memset(temp, '\0', sizeof(temp));
		fseek(fr, c_Addr, SEEK_SET);
		fread(temp, BLOCK_SIZE, 1, fr);
		fflush(fr);
		fseek(bfw, c_Addr, SEEK_SET);
		fwrite(temp, sizeof(temp), 1, bfw);
		fflush(bfw);
		c_Addr += BLOCK_SIZE;
		//printf("%d\n", c_Addr);
	}
	
	
	//char originbuf[Disk_Size];
	//fseek(fr, 0, SEEK_SET);
	//fread(originbuf, sizeof(originbuf), 1, fr);
	//fflush(fr);

	//fseek(bfw, 0, SEEK_SET);
	//fwrite(originbuf, sizeof(originbuf), 1, bfw);
	
	//���λͼ���
	char tmp_inodeBitmap[INODE_NUM];
	memset(tmp_inodeBitmap, 0, sizeof(tmp_inodeBitmap));
	for (int i = 0; i < INODE_NUM; i++) {
		tmp_inodeBitmap[i] = 0;
	}

	fseek(fw, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	fwrite(tmp_inodeBitmap, sizeof(tmp_inodeBitmap), 1, fw);
	fflush(fw);

	time_t nowtime;
	time(&nowtime);
	last_backup_time = nowtime;
	fclose(bfw);
	fclose(bfr);
	return true;
}

//����ת��
bool incrementalBackup() {
	//�����µ�sys�ļ�
	time_t cur_time;
	char backupSysName[256] = { 0 };
	time(&cur_time);
	strftime(backupSysName, sizeof(backupSysName), "IncreBackupSys %Y.%m.%d %H-%M-%S.sys", localtime(&cur_time));
	string str(backupSysName);
	printf("%s", backupSysName);
	if ((bfr = fopen(backupSysName, "rb")) == NULL) {
		bfw = fopen(backupSysName, "wb");
		if (bfw == NULL) {
			printf("BackupSys File error��");
			return false;;
		}
		bfr = fopen(backupSysName, "rb");
		printf("BackupSys File successful open");
	}
	else {
		bfw = fopen(backupSysName, "rb+");
		if (bfw == NULL) {
			printf("BackupSys File error��");
			return false;
		}
	}

	//������һ������ת�������޸ĵ�inode������ı䣩
	char new_modify_inodeBitmap[INODE_NUM];
	memset(new_modify_inodeBitmap,0, sizeof(new_modify_inodeBitmap));
	//��ÿһ���޸Ĺ����ļ�&ȫ��Ŀ¼��λͼ�������
	for (int i = 0; i < INODE_NUM; i++) {
		inode tmp_inode;
		fseek(fr, Inode_Start_Addr+i*sizeof(inode), SEEK_SET);
		fread(&tmp_inode, sizeof(inode), 1, fr);
		if (tmp_inode.file_modified_time > last_backup_time) {
			new_modify_inodeBitmap[i] = 1;
		}
		else {
			continue;
		}
	}
	//�ڱ����ļ��м�¼����ı�
	fseek(bfw, Backup_Start_Addr, SEEK_SET);
	fwrite(new_modify_inodeBitmap, sizeof(new_modify_inodeBitmap), 1, bfw);

	//������һ�ε�modify_inodeBitmap
	char modify_inodeBitmap[INODE_NUM];
	memset(modify_inodeBitmap, 0, sizeof(modify_inodeBitmap));
	fseek(fr, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	fread(modify_inodeBitmap, sizeof(modify_inodeBitmap), 1, fr);
	for (int i = 0; i < INODE_NUM; i++) {
		modify_inodeBitmap[i] |= new_modify_inodeBitmap[i];
	}
	//���£���OS�м�¼modified inode bitmap
	fseek(fw, Modified_inodeBitmap_Start_Addr, SEEK_SET);
	fwrite(modify_inodeBitmap, sizeof(modify_inodeBitmap), 1, fw);
	

	//ȥÿһ�����޸ĵ�inode���ұ���Ӧ�޸ĵ�ֱ�ӿ飨����ı䣩
	int Cur_Addr=Backup_Block_Start_Addr;
	for (int i = 0; i < INODE_NUM; i++) {
		inode tmp_inode;
		if (new_modify_inodeBitmap[i] == 1) {
			char tmp[128];
			memset(tmp, '\0', sizeof(tmp));
			//���޸Ĺ���inode
			fseek(fr, Inode_Start_Addr+i*INODE_SIZE, SEEK_SET);//?
			fread(&tmp_inode, sizeof(inode), 1, fr);
			fflush(fr);
			//���޸Ĺ���inodeд����������ϵͳ
			fseek(bfw, Cur_Addr, SEEK_SET);
			fwrite(&tmp_inode, sizeof(inode), 1, bfw);
			fflush(bfw);
			Cur_Addr += sizeof(inode);
			//��inodeֱ�ӿ�
			//int tmp_i_dirBlock[10];
			for (int j = 0; j < 10; j++) {
				if (tmp_inode.i_dirBlock[j] != -1) {
					//��ֱ�ӿ�
					char tmp_block[BLOCK_SIZE];
					memset(tmp_block, '\0', sizeof(tmp_block));
					int block_addr = tmp_inode.i_dirBlock[j];
					fseek(fr, block_addr, SEEK_SET);
					fread(tmp_block, sizeof(tmp_block), 1, fr);
					fflush(fr);
					fseek(bfw, Cur_Addr, SEEK_SET);
					fwrite(tmp_block, sizeof(tmp_block), 1, bfw);
					Cur_Addr += sizeof(tmp_block);
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
	//��ȡ��ǰ�ļ����µ������ļ�
	DIR* dir;
	struct dirent* ent;
	//�ָ�ȫ������
	if ((dir = opendir("./")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			printf("%s\t", ent->d_name);
			printf("\n");
			if (strncmp(ent->d_name, "Full",4) == 0) {
				printf("%s", ent->d_name);
				//���ļ�
				if ((bfr = fopen(ent->d_name, "rb")) == NULL) {
					bfw = fopen(ent->d_name, "wb");
					if (bfw == NULL) {
						printf("Error��");
						return false;;
					}
					bfr = fopen(ent->d_name, "rb");
					printf("Successful");
				}
				else {
					bfw = fopen(ent->d_name, "rb+");
					if (bfw == NULL) {
						printf("Error");
						return false;
					}
				}
				
				//�ӱ��ݵ��ļ��ж�
				//memset(buffer, '\0', sizeof(buffer));
				//fseek(bfr, 0, SEEK_SET);
				//fread(buffer, sizeof(buffer), 1, bfr);
				char buffer[Disk_Size];
				char temp[BLOCK_SIZE];
				memset(buffer, '\0', sizeof(buffer));
				int c_Addr = 0;
				int num = Disk_Size / BLOCK_SIZE;
				for (int i = 0; i < num; i++) {
					/*if (c_Addr == 11904) {
						inode tmp;
						fseek(fr, c_Addr, SEEK_SET);
						fread(&tmp, sizeof(inode), 1, fr);
						printf("%s", tmp.i_gname);
					}*/
					memset(temp, '\0', sizeof(temp));
					fseek(bfr, c_Addr, SEEK_SET);
					fread(temp, BLOCK_SIZE, 1, bfr);
					fflush(bfr);
					fseek(fw, c_Addr, SEEK_SET);
					fwrite(temp, sizeof(temp), 1, fw);
					fflush(fw);
					c_Addr += BLOCK_SIZE;
					//printf("%d\n", c_Addr);
				}
				break;
			}
			else {
				continue;
			}

		}
	}

	//����������ת��
	//ͳ���ж��ٸ�����ת��
	vector<string> files;
	int incre_backup_count = 0;
	if ((dir = opendir("./")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (strncmp(ent->d_name, "Incre",5) == 0) {
				incre_backup_count++;
				files.push_back(ent->d_name);
			}
		}
	}
	//���޸�ʱ����絽�����򣨴�С����
	int size = files.size();
	for (int m = 0; m < size - 1; ++m) {
		for (int n = 0; n < size - 1; ++n) {
			if (strcmp(files[n].c_str(), files[n + 1].c_str()) > 0) {
				string tmp = files[n];
				files[n] = files[n + 1];
				files[n + 1] = tmp;
			}
		}
	}

	//���絽��˳�������ת�����ļ�
	for (int i = 0; i < incre_backup_count; i++) {
		//�򿪱����ļ�
		if ((bfr = fopen(files[i].c_str(), "rb")) == NULL) {
			bfw = fopen(files[i].c_str(), "wb");
			if (bfw == NULL) {
				printf("�����ļ���ʧ�ܣ�");
				return false;;
			}
			bfr = fopen(files[i].c_str(), "rb");
			printf("�����ļ��򿪳ɹ�");
		}
		else {
			bfw = fopen(files[i].c_str(), "rb+");
			if (bfw == NULL) {
				printf("�����ļ���ʧ��");
				return false;
			}
		}

		int Cur_Addr = 0;
		//ȡ�����ļ��е�inodeλͼ
		char tmp_bitmap[1024];
		memset(tmp_bitmap, '\0', sizeof(tmp_bitmap));
		fseek(bfr, Cur_Addr, SEEK_SET);
		fread(&tmp_bitmap, sizeof(tmp_bitmap), 1, bfr);
		Cur_Addr += sizeof(tmp_bitmap);
		for (int k = 0; k < INODE_NUM; k++) {
			if (tmp_bitmap[k] == 1) {
				inode tmp;
				//��inode�����������ļ�ϵͳ�ж���
				fseek(bfr, Cur_Addr, sizeof(inode));
				fread(&tmp, sizeof(inode), 1, bfr);
				Cur_Addr += sizeof(inode);
				//��inodeд��ԭ�ļ�ϵͳ
				fseek(fw, Inode_Start_Addr + k * sizeof(inode), SEEK_SET);
				fwrite(&tmp, sizeof(inode), 1, fw);
				//����ֱ�ӿ�
				for (int i = 0; i < 10; i++) {
					if (tmp.i_dirBlock[i] != -1) {
						//�ѱ����ļ��е�ֱ�ӿ����
						char tmp_block[512];
						memset(tmp_block, '\0', sizeof(tmp_block));
						fseek(bfr, Cur_Addr, SEEK_SET);
						fread(&tmp_block, sizeof(tmp_block), 1, bfr);
						Cur_Addr += sizeof(tmp_block);
						//д��Դ�ļ���Ӧ���ַ
						fseek(fw, tmp.i_dirBlock[i], SEEK_SET);
						fwrite(&tmp_block, sizeof(tmp_block), 1, fw);
					}
				}
			}
		}
	}
	return true;
}