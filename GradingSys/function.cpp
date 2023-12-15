#include"os.h"
#include"function.h"
#include<cstring>
#include<cstdio>
#include<mutex>
std::mutex workPrt;
using namespace std;

void help(Client & client)
{
	char help[10]; memset(help, '\0', 10);
	if (strcmp(client.Cur_Group_Name, "student") == 0)
		strcpy(help, "help1");
	else if (strcmp(client.Cur_Group_Name, "teacher") == 0)
		strcpy(help, "help2");
	else strcpy(help, "help");
	send(client.client_sock, help, strlen(help), 0);
}

bool cd_func(Client& client, int CurAddr, char* str) {
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310]; memset(pro_cur_dir_name, '\0', 310);
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);
	int flag = 1;

	//查看cd类型：绝对路径or相对路径
	if (strcmp(str, "/") == 0) {//前往根目录
		gotoRoot(client);
		return true;
	}
	if (str[0] == '/') {	//绝对路径
		gotoRoot(client);
		str += 1;
	}
	while (strlen(str) != 0) {
		char name[sizeof(str)];
		int i = 0;
		memset(name, '\0', sizeof(name));
		while (((*str) != '/') && (strlen(str) != 0)) {
			name[i++] = str[0];
			str += 1;
		}
		if ((*str) == '/') str += 1;
		if (strlen(name) != 0) {
			if (cd(client, client.Cur_Dir_Addr, name) == false) {
				flag = 0;
				break;
			}
		}
	}

	//判断是否成功
	if (flag == 0) {//失败，恢复现场
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	return true;
}

bool mkdir_func(Client& client, int CurAddr, char* str) {//在任意目录下创建目录
	//绝对,相对,直接创建
	char* p = strrchr(str, '/');
	if (p == NULL) {	//直接创建
		if (mkdir(client, CurAddr, str)) { return true; }
		else { return false; }
	}
	else {
		char name[File_Max_Size];
		memset(name, '\0', sizeof(name));
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(client, CurAddr, str)) {
			if (mkdir(client, client.Cur_Dir_Addr, name))
				return true;
		}
		return false;
	}
}
bool rm_func(Client& client, int CurAddr, char* str, char* s_type) {//在任意目录下删除
	//文件类型
	int type = -1;
	if (strcmp(s_type, "-rf") == 0) {
		type = 1;
	}
	else if (strcmp(s_type, "-f") == 0) {
		type = 0;
	}
	else {
		//printf("无法确认文件删除类型，请重新输入！\n");
		char ms[] = "Cannot determine the file type, please re-enter!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//绝对,相对,直接创建
	char* p = strrchr(str, '/');
	if (p == NULL) {	//直接删除
		if (rm(client, CurAddr, str, type))	return true;
		return false;
	}
	else {
		char name[File_Max_Size];
		memset(name, '\0', sizeof(name));
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(client, CurAddr, str)) {
			if (rm(client, client.Cur_Dir_Addr, name, type))
				return true;
		}
		return false;
	}
}
bool touch_func(Client& client, int CurAddr, char* str, char* buf) {//在任意目录下创建文件
	//绝对,相对,直接创建
	char* p = strrchr(str, '/');
	if (p == NULL) {	//直接创建
		if (mkfile(client, CurAddr, str, buf))	return true;
		return false;
	}
	else {
		char name[File_Max_Size];
		memset(name, '\0', sizeof(name));
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(client, CurAddr, str)) {
			if (mkfile(client, client.Cur_Dir_Addr, name, buf))
				return true;
		}
		return false;
	}
}

bool echo_func(Client& client, int CurAddr, char* str, char* s_type, char* buf) {//在任意目录下创建or覆盖写入or追加
	//判断类型 0：覆盖写入 1：追加
	int type = -1;
	if (strcmp(s_type, ">") == 0) {
		type = 0;
	}
	else if (strcmp(s_type, ">>") == 0) {
		type = 1;
	}
	else {
		//printf("echo输入格式错误，请输入正确格式!\n");
		char ms[] = "Error input format for 'echo' command, please try again!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	//寻找直接地址
	char* p = strrchr(str, '/');
	char name[File_Max_Size];
	memset(name, '\0', sizeof(name));
	if (p != NULL) {
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(client, CurAddr, str) == false) {
			return false;
		}
	}
	else {
		strcpy(name, str);
	}

	//类型执行
	if (echo(client, client.Cur_Dir_Addr, name, type, buf))	return true;
	return false;
}

bool chmod_func(Client& client, int CurAddr, char* pmode, char* str) {
	//寻找直接地址
	char* p = strrchr(str, '/');
	char name[File_Max_Size];
	memset(name, '\0', sizeof(name));
	if (p != NULL) {
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(client, CurAddr, str) == false) {
			return false;
		}
	}
	else {
		strcpy(name, str);
	}

	//类型执行
	if (chmod(client, CurAddr, name, pmode))	return true;
	return false;
}
bool chown_func(Client& client, int CurAddr, char* u_g, char* str) {
	//寻找直接地址
	char* p = strrchr(str, '/');
	char file[File_Max_Size];
	memset(file, '\0', sizeof(file));
	if (p != NULL) {
		p++;
		strcpy(file, p);
		*p = '\0';
		if (cd_func(client, CurAddr, str) == false) {
			return false;
		}
	}
	else {
		strcpy(file, str);
	}
	//获取用户和用户组
	p = strstr(u_g, ":");
	char name[20], group[20];
	memset(name, '\0', strlen(name));
	memset(group, '\0', strlen(group));
	if (p == NULL) {
		strcpy(name, u_g);
	}
	else {
		strncpy(name, u_g, p - u_g);
		p += 1;
		strcpy(group, p);
	}
	if (chown(client, CurAddr, file, name, group))	return true;
	return false;
}
bool passwd_func(Client& client, char* username) {
	if ((strcmp(client.Cur_Group_Name, "root") != 0) && (strlen(username) != 0)) {
		//printf("普通用户无法修改其他用户密码\n");
		char ms[] = "Only root can change user's password!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		return false;
	}

	char pwd[100];
	char re_pwd[100];

	char ms[100]; memset(ms, '\0', sizeof(ms));
	sprintf(ms, "Changing password for user %s\n", client.Cur_User_Name);
	send(client.client_sock, ms, strlen(ms), 0);

	memset(ms, '\0', sizeof(ms));
	sprintf(ms, "New password: ");
	send(client.client_sock, ms, strlen(ms), 0);
	recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(pwd, client.buffer);

	memset(ms, '\0', sizeof(ms));
	sprintf(ms, "Retype new password:");
	send(client.client_sock, ms, strlen(ms), 0);
	recv(client.client_sock, client.buffer, sizeof(client.buffer), 0);
	strcpy(re_pwd, client.buffer);

	if (strcmp(pwd, re_pwd) == 0) {
		if (passwd(client, username, pwd) == 0) {
			return true;
		}
	}
	return false;
}

void cmd(Client& client)
{
	char com1[100];
	char com2[100];
	char com3[100];
	char cmd_str[BUF_SIZE] = "";
	strcpy(cmd_str, client.buffer);
	sscanf(cmd_str, "%s", com1);
	//一级传来com1和com2
	if (strcmp(com1, "help") == 0) {
		help(client);
	}
	else if (strcmp(com1, "ls") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		ls(client, com2);
	}
	else if (strcmp(com1, "cd") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		cd_func(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "gotoRoot") == 0) {
		gotoRoot(client);
	}
	else if (strcmp(com1, "mkdir") == 0) {	//cd至父目录--> mkdir
		sscanf(cmd_str, "%s%s", com1, com2);
		mkdir_func(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "rm") == 0) {
		sscanf(cmd_str, "%s%s%s", com1, com2, com3);
		rm_func(client, client.Cur_Dir_Addr, com3, com2);
	}
	else if (strcmp(com1, "touch") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		touch_func(client, client.Cur_Dir_Addr, com2, "");
	}
	else if (strcmp(com1, "echo") == 0) {
		//注意文字里面不要有空格
		char com4[100];
		sscanf(cmd_str, "%s%s%s%s", com1, com2, com3, com4);
		echo_func(client, client.Cur_Dir_Addr, com4, com3, com2);
	}
	else if (strcmp(com1, "cat") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		cat(client, client.Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "chmod") == 0) {
		sscanf(cmd_str, "%s%s%s", com1, com2, com3);
		chmod_func(client, client.Cur_Dir_Addr, com2, com3);
	}
	else if (strcmp(com1, "chown") == 0) {
		sscanf(cmd_str, "%s%s%s", com1, com2, com3);
		chown_func(client, client.Cur_Dir_Addr, com2, com3);
	}

	else if (strcmp(com1, "useradd") == 0) {
		//useradd -g group -m user
		char group[100];
		char user[100];
		char passwd[100];
		sscanf(cmd_str, "%s%s%s%s%s", com1, com2, group, com3, user);
		if ((strcmp(com2, "-g") != 0) || ((strcmp(com3, "-m") != 0))) {
			//printf("命令格式错误!\n");
			char ms[] = "Command format error!\n";
			send(client.client_sock, ms, strlen(ms), 0);
			return;
		}
		inPasswd(client, passwd);
		useradd(client, user, passwd, group);
	}
	else if (strcmp(com1, "userdel") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		userdel(client, com2);
	}
	else if (strcmp(com1, "groupadd") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		groupadd(client, com2);
	}
	else if (strcmp(com1, "groupdel") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		groupdel(client, com2);
	}
	else if (strcmp(com1, "passwd") == 0) {
		if (sscanf(cmd_str, "%s%s", com1, com2) == 1) {
			passwd_func(client, "");
		}
		else {
			passwd_func(client, com2);
		}
	}
	else if (strcmp(com1, "logout") == 0) {
		logout(client);
	}

	
	else if (strcmp(com1, "exit") == 0) {
		//cout << "退出成绩管理系统，拜拜！" << endl;
		char ms[] = "Exitting our Grading System..... See you!\n";
		send(client.client_sock, ms, strlen(ms), 0);
		exit(0);
	}
  //root组特有
	if (strcmp(client.Cur_Group_Name, "root") == 0) {
		if (strcmp(com1, "batchadd") == 0) {
			sscanf(cmd_str, "%s", com1);
			add_users(sys, STUDENT_COURSE_LIST);
		}
		//备份系统&恢复系统
		else if (strcmp(com1, "fullbackup") == 0) {
			fullBackup();
		}
		else if (strcmp(com1, "recovery") == 0) {
			recovery();
		}
		else if (strcmp(com1, "increbackup") == 0) {
			incrementalBackup();
		}
	}
	
	//teacher组特有
	if (strcmp(client.Cur_Group_Name, "teacher") == 0) {
		if (strcmp(com1, "publish_task") == 0) {
			sscanf(cmd_str, "%s%s%s", com1, com2, com3);
			publish_task(client, com2, com3);
		}
		else if (strcmp(com1, "judge_hw") == 0) {
			sscanf(cmd_str, "%s%s%s", com1, com2, com3);
			judge_hw(client, STUDENT_COURSE_LIST, com2, com3);
		}
	}
  
  //student组特有
	if (strcmp(client.Cur_Group_Name, "student") == 0) {
		if (strcmp(com1, "check_hw_content") == 0) //check desription
		{
			// check lesson hw
			sscanf(cmd_str, "%s%s%s", com1, com2, com3);
			check_hw_content(client, com2, com3);
		}
		else if (strcmp(com1, "check_hw_score") == 0)
		{
			// check_hw_score lesson hw
			sscanf(cmd_str, "%s%s%s", com1, com2, com3);
			check_hw_score(client, com2, com3);
		}
		else if (strcmp(com1, "submit_hw_to") == 0)
		{
			// submit_hw_to lesson hwname
			sscanf(cmd_str, "%s%s%s", com1, com2, com3);
			submit_assignment(client, client.Cur_User_Name, com2, com3);
		}
	}
	return;
}
