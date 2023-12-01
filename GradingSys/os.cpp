#include"os.h"
#include<iomanip>

using namespace std;

void help() {
	
	cout.setf(ios::left); //设置对齐方式为left 
	cout.width(30); //设置宽度，不足用空格填充 
	//cout << setiosflags(ios::left);
	cout << "ls" << "Display the current directory listing" << endl;	//列出当前目录清单
	cout.width(30);
	cout << "cd" << "Enter the specific directory " << endl;		//前往指定目录
	cout.width(30);
	cout << "mkdir" << "Create directory" << endl;					//创建目录
	cout.width(30);
	cout << "rm" << "Delete the file or directory" << endl;			//删除文件和目录 
	cout.width(30);
	cout << "touch" << "Create new file" << endl;				//创建新文件
	cout.width(30);
	cout << "read" << "Read the content of file" << endl;		//读文件
	cout.width(30);
	cout << "write" << "Write the file" << endl;			//写文件
	cout.width(30);
	cout << "chmod" << "Modify the access right" << endl;		//修改文件权限
	cout.width(30);
	cout << "adduser" << "Add user" << endl;		//新增用户
	cout.width(30);
	cout << "deluser" << "Delete user" << endl;		//删除用户
	cout.width(30);
	cout << "addusergrp" << "Add user group" << endl;		//新增用户组
	cout.width(30);
	cout << "delusergrp" << "Delete user group" << endl;		//删除用户组
	cout.width(30);
	cout << "snapshot" << "Back up the system" << endl;			//备份系统
	cout.width(30);
	cout << "recover" << "Recover the system" << endl;			//恢复系统
}

bool Format() {
	return true;
}

void inUsername(char username[])	//输入用户名
{
	printf("username:");
	scanf("%s", username);	//用户名
}

void inPasswd(char passwd[])	//输入密码
{
	printf("passwd:");
	scanf("%s", passwd);
}

bool login()	//登陆
{
	char username[100] = { 0 };
	char passwd[100] = { 0 };
	inUsername(username);	//输入用户名
	inPasswd(passwd);		//输入用户密码
	if (strcmp(username,"root")==0&& strcmp(passwd, "root")==0) {	//核对用户名和密码
		isLogin = true;
		return true;
	}
	else {
		isLogin = false;
		return false;
	}
}

bool mkdir(int parinodeAddr, char name[]) {

}

void cmd(char cmd[]) {
	char com1[100];
	char com2[100];
	char com3[100];
	sscanf(cmd,"%s", com1);
	if (strcmp(com1, "ls") == 0) {
		ls(Cur_Dir_Addr);
	}
	else if (strcmp(com1, "mkdir") == 0) {
		cout << "in mkdir" << endl;
		sscanf(cmd, "%s%s", com1, com2);
		cout << com2 << endl;
		mkdir(Cur_Dir_Addr, com2);
	}
	return;
}

void ls(int parinodeAddr) {
	return;
}