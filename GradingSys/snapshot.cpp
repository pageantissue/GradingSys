#include"snapshot.h"
#include"os.h"

void initial() {
	//把备份的空间初始化
	char backup_buf[500000];
	memset(backup_buf, '\0', sizeof(backup_buf));
	fseek(bfw, 0, SEEK_SET);
	fwrite(backup_buf, sizeof(backup_buf), 1, bfw);
	return;
}
bool backup(int count,int addr) {
	if (count == -1) {
		return true;
	}
	if ((bfr = fopen(BACKUP_SYS_NAME, "rb")) == NULL) {
		bfw = fopen(BACKUP_SYS_NAME, "wb");
		if (bfw == NULL) {
			printf("备份文件打开失败！");
			return false;
		}
		bfr = fopen(BACKUP_SYS_NAME, "rb");
		printf("备份文件打开成功");
	}
	//备份inodebitmap
	char tmp_inodeBitMap[1024];
	fseek(fr, InodeBitmap_Start_Addr, SEEK_SET);
	fread(&tmp_inodeBitMap, sizeof(tmp_inodeBitMap), 1, fr);
	fseek(bfw, count % Operation_Num * (43 * BLOCK_SIZE), SEEK_SET);
	fwrite(&tmp_inodeBitMap, sizeof(tmp_inodeBitMap), 1, bfw);

	return true;
}

bool recovery(int count) {
	return true;
}