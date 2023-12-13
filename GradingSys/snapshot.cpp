#include"snapshot.h"
#include"os.h"

void initial() {
	if ((bfr = fopen(BACKUP_SYS_NAME, "rb")) == NULL) {
		bfw = fopen(BACKUP_SYS_NAME, "wb");
		if (bfw == NULL) {
			printf("备份文件打开失败！");
			return;
		}
		bfr = fopen(BACKUP_SYS_NAME, "rb");
		printf("备份文件打开成功");
	}
	else {
		bfw = fopen(BACKUP_SYS_NAME, "rb+");
		if (bfw == NULL) {
			printf("备份文件打开失败");
			return;
		}
	}
	//把备份的空间初始化
	char backup_buf[500000];
	memset(backup_buf, '\0', sizeof(backup_buf));
	fseek(bfw, Start_Addr, SEEK_SET);
	fwrite(backup_buf, sizeof(backup_buf), 1, bfw);
	return;
}
bool backup(int count,int parAddr,int chidAddr) {
	if (count == -1) {
		return true;
	}
	Backup tmp_backup;
	//备份inodebitmap
	char tmp_inodeBitMap[1024];
	fseek(fr, InodeBitmap_Start_Addr, SEEK_SET);
	fread(&tmp_inodeBitMap, sizeof(tmp_inodeBitMap), 1, fr);
	fseek(bfw, count % Operation_Num * (Backup_Block_Num * BLOCK_SIZE), SEEK_SET);
	fwrite(&tmp_inodeBitMap, sizeof(tmp_inodeBitMap), 1, bfw);
	
	//备份blcokbitmap
	char tmp_blockBitMap[10240];
	fseek(fr, BlockBitmap_Start_Addr, SEEK_SET);
	fread(&tmp_blockBitMap, sizeof(tmp_blockBitMap), 1, fr);
	fseek(bfw, (count % Operation_Num * (Backup_Block_Num * BLOCK_SIZE) + sizeof(tmp_inodeBitMap)), SEEK_SET);
	fwrite(&tmp_blockBitMap, sizeof(tmp_blockBitMap), 1, bfw);


	//备份parinode
	inode grad_parinode;
	//inode tmp_parinode;
	fseek(fr, parAddr, SEEK_SET);
	fread(&grad_parinode, sizeof(inode), 1, fr);
	fseek(bfw, (count % Operation_Num * (Backup_Block_Num * BLOCK_SIZE)) + sizeof(tmp_inodeBitMap) + sizeof(tmp_blockBitMap), SEEK_SET);
	fwrite(&grad_parinode, sizeof(grad_parinode), 1, bfw);


	//备份childinode
	inode grad_childinode;
	//inode tmp_childinode;
	fseek(fr, chidAddr, SEEK_SET);
	fread(&grad_childinode, sizeof(inode), 1, fr);
	fseek(bfw, (count % Operation_Num * (Backup_Block_Num * BLOCK_SIZE)) + sizeof(tmp_inodeBitMap) + sizeof(tmp_blockBitMap)+sizeof(inode), SEEK_SET);
	fwrite(&grad_childinode, sizeof(grad_childinode), 1, bfw);

	//备份parinode直接块
	int baddr = -1;
	for (int i = 0; i < 10; i++) {
		baddr = grad_parinode.i_dirBlock[i];
		char tmp_block[512];

	}



	return true;
}

bool recovery(int count) {
	return true;
}