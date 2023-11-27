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
	int plen = 0;
	char c;
	fflush(stdin);	//清空缓冲区
	printf("passwd:");
	while (c = getchar()) {
		if (c == '\r') {	//输入回车，密码确定
			passwd[plen] = '\0';
			fflush(stdin);	//清缓冲区
			printf("\n");
			break;
		}
		else if (c == '\b') {	//退格，删除一个字符
			if (plen != 0) {	//没有删到头
				plen--;
			}
		}
		else {	//密码字符
			passwd[plen++] = c;
		}
	}
}

bool login()	//登陆界面
{
	char username[100] = { 0 };
	char passwd[100] = { 0 };
	inUsername(username);	//输入用户名
	inPasswd(passwd);		//输入用户密码
	printf("到这1");
	//if (strcmp(username,"root")==0&& strcmp(passwd, "root")==0) {	//核对用户名和密码
	if(1){
		printf("到这2");
		isLogin = true;
		return true;
	}
	else {
		printf("到这3");
		isLogin = false;
		return false;
	}
}
