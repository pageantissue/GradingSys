#include"os.h"
#include"function.h"
#include<iomanip>
#include<time.h>
#include<string.h>
#include<stdio.h>
#include<iostream>

using namespace std;

void help() {
	cout.setf(ios::left); //设置对齐方式为left 
	cout.width(30); //设置宽度，不足用空格填充 
	cout << "ls" << "Display the current directory listing" << endl;	//列出当前目录清单(ls/ls -l)
	cout.width(30);
	cout << "cd" << "Enter the specific directory " << endl;		//前往指定目录(cd home)
	cout.width(30);
	cout << "gotoRoot" << "Return to the root directory " << endl;		//返回根目录
	cout.width(30);
	cout << "mkdir" << "Create directory" << endl;					//创建目录
	cout.width(30);
	cout << "rm" << "Delete directory or file" << endl;					//删除目录或文件
	cout.width(30);
	cout << "touch" << "Create a blank file" << endl;			//创建空文件
	cout.width(30);
	cout << "echo" << "Create a non-empty file" << endl;		//创建含内容文件
	cout.width(30);
	cout << "writefile" << "write a blank file" << endl;		//写入文件
	cout.width(30);
	cout << "addfile" << "continue to write file" << endl;		//续写文件
	cout.width(30);
	cout << "chmod" << "Modify the access right" << endl;    //修改文件权限
	cout.width(30);
	//cat，chown

	cout << "useradd" << "Add user" << endl;		//新增用户
	cout.width(30);
	cout << "userdel" << "Delete user" << endl;		//删除用户
	cout.width(30);

	cout << "logout" << "Logout the account" << endl;		//退出账号
	cout.width(30);
	//usergrpadd,userfrpdel,密码修改，


	cout << "snapshot" << "Back up the system" << endl;			//备份系统
	cout.width(30);
	//备份系统&恢复系统

	cout << "exit" << "Exit the system" << endl;		//退出系统
}

bool cd_func(int CurAddr, char* str) {	
	//cd至任一绝对路径or相对路径
	//保存现场（失败恢复）
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	strcpy(pro_cur_dir_name, Cur_Dir_Name);
	int flag = 1;
	
	//查看cd类型：绝对路径or相对路径
	if (strcmp(str, "/") == 0) {//前往根目录
		gotoRoot();
		return true;
	}
	if (str[0] == '/') {	//绝对路径
		gotoRoot();
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
		if((*str) == '/') str += 1;
		if (strlen(name) != 0) {
			if (cd(Cur_Dir_Addr, name)==false) {
				flag = 0;
				break;
			}
		}
		else {
			break;
		}
	}

	//判断是否成功
	if (flag == 0) {//失败，恢复现场
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	return true;
}
bool mkdir_func(int CurAddr, char* str) {//在任意目录下创建目录
	//绝对,相对,直接创建
	char* p = strrchr(str, '/');
	if (p == NULL) {	//直接创建
		if (mkdir(CurAddr, str))	return true;
		return false;
	}
	else {
		char name[File_Max_Size];
		memset(name, '\0', sizeof(name));
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(CurAddr, str)) {
			if (mkdir(Cur_Dir_Addr, name))
				return true;
		}
		return false;
	}
}
bool mkfile_func(int CurAddr, char* str,char *buf) {//在任意目录下创建文件
	//绝对,相对,直接创建
	char* p = strrchr(str, '/');
	if (p == NULL) {	//直接创建
		if (mkfile(CurAddr, str,buf))	return true;
		return false;
	}
	else {
		char name[File_Max_Size];
		memset(name, '\0', sizeof(name));
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(CurAddr, str)) {
			if (mkfile(Cur_Dir_Addr, name,buf))
				return true;
		}
		return false;
	}
}
bool rm_func(int CurAddr, char* str,char* s_type) {//在任意目录下删除
	//文件类型
	int type = -1;
	if (strcmp(s_type, "-rf") == 0) {
		type = 1;
	}
	else if (strcmp(s_type, "-f") == 0) {
		type = 0;
	}
	else {
		printf("无法确认文件删除类型，请重新输入！\n");
		return false;
	}

	//绝对,相对,直接创建
	char* p = strrchr(str, '/');
	if (p == NULL) {	//直接删除
		if (rm(CurAddr, str,type))	return true;
		return false;
	}
	else {
		char name[File_Max_Size];
		memset(name, '\0', sizeof(name));
		p++;
		strcpy(name, p);
		*p = '\0';
		if (cd_func(CurAddr, str)) {
			if (rm(Cur_Dir_Addr, name,type))
				return true;
		}
		return false;
	}
}

void cmd(char cmd_str[], int count) {
	char com1[100];
	char com2[100];
	char com3[100];
	char com4[100];
	sscanf(cmd_str, "%s", com1);
	//一级传来com1和com2
	if (strcmp(com1, "help") == 0) {
		help();
	}
	else if (strcmp(com1, "ls") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		ls(com2);
	}
	else if (strcmp(com1, "cd") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		cd_func(Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "gotoRoot") == 0) {
		gotoRoot();
	}
	else if (strcmp(com1, "mkdir") == 0) {	//cd至父目录--> mkdir
		sscanf(cmd_str, "%s%s", com1, com2);
		mkdir_func(Cur_Dir_Addr, com2);
	}
	else if (strcmp(com1, "rm") == 0) {
		sscanf(cmd_str, "%s%s%s", com1, com2,com3);
		rm_func(Cur_Dir_Addr,com3,com2);
	}
	else if (strcmp(com1, "touch") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		mkfile_func(Cur_Dir_Addr, com2, "");
	}
	else if (strcmp(com1, "echo") == 0) {
		sscanf(cmd_str, "%s%s%s%s", com1, com2,com3,com4);
		//去掉括号
		com2[-1] = '\0';
		mkfile_func(Cur_Dir_Addr, com4,com2+1);
	}
	//writefile&addfile需要改一下
	//cat,chown
	//chmod

	else if (strcmp(com1, "useradd") == 0) {
		inUsername(com1);
		inPasswd(com2);
		ingroup(com3);
		useradd(com1, com2, com3);
	}
	else if (strcmp(com1, "userdel") == 0) {
		sscanf(cmd_str, "%s%s", com1, com2);
		userdel(com2);
	}
	else if (strcmp(com1, "logout") == 0) {
		logout();
	}
	//usergrpadd,userfrpdel,密码修改，

	//备份系统&恢复系统
	else if (strcmp(com1, "exit") == 0) {
		cout << "退出成绩管理系统，拜拜！" << endl;
		exit(0);
	}

	return;
}