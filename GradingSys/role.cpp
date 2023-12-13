#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string.h>
#include<fstream>
#include"function.h"
#include"os.h"
#include"root.h"
using namespace std;
bool add_users(char * namelist) { //root：批量创建教师及学生用户
	//验证身份
	if (strcmp(Cur_Group_Name, "root") != 0) {
		printf("仅管理员可批量增删用户\n");
		return false;
	}
	//备份信息
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	//保存名单
	ifstream fin(namelist);
	if (!fin.is_open()) {
		cout << "无法打开名单" << endl;
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	string line;
	Relation relations[1000];
	int i = 0;
	char pwd[30];
	while (getline(fin, line)) {
		Relation rela;
		sscanf(line.c_str(), "%s:%s:%s", rela.student, rela.lesson, rela.teacher);
		relations[i++] = rela;
	}

	//根据名单创建
	//设置密码默认与姓名同名
	for (int j = 0; j < i; j++) {
		//创建用户及其文件夹
		strcpy(pwd, relations[j].student);
		useradd(relations[j].student, "student", pwd);
		strcpy(pwd, relations[j].teacher);
		useradd(relations[j].teacher, "teacher", pwd);
		//在其文件夹下创建对应课程文件夹
		char dir_path[100];
		sprintf(dir_path, "/home/%s/%s", relations[j].teacher, relations[j].lesson);
		mkdir_func(Cur_Dir_Addr, dir_path);
		sprintf(dir_path, "/home/%s/%s", relations[j].student, relations[j].lesson);
		mkdir_func(Cur_Dir_Addr, dir_path);
	}
}

bool publish_task(char* lesson,char* filename) {//教师：发布本次作业任务
	//验证身份
	if (strcmp(Cur_Group_Name, "teacher") != 0) {
		printf("只有老师可以发布作业!\n");
		return false;
	}

	//备份信息
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	//读取file信息
	char buf[BLOCK_SIZE * 10];
	string line;
	memset(buf, '\0', sizeof(buf));
	ifstream fin(filename);
	if (!fin.is_open()) {
		cout << "无法打开名单" << endl;
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	while (getline(fin, line)) {
		strcat(buf, line.c_str());
	}

	//将file复制到虚拟OS中
	char dir_path[100];
	sprintf(dir_path, "/home/%s/%s/%s_description", Cur_User_Name, lesson, filename);
	echo_func(Cur_Dir_Addr, dir_path, ">", buf); //新建task文件并发布

	return true;
}

bool judge_hw(char* namelist, char* lesson, char* hwname) {//教师：评价本次作业
	//验证身份
	if (strcmp(Cur_Group_Name, "teacher") != 0) {
		printf("只有老师可以点评作业!\n");
		return false;
	}

	//备份信息
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	//新建本次作业评价文档( sname : mark)
	char score_path[310];
	sprintf(score_path, "/home/%s/%s/%s_score", Cur_Group_Name, lesson, hwname);
	touch_func(Cur_Dir_Addr, score_path, "");

	//保存名单
	ifstream fin(namelist);
	if (!fin.is_open()) {
		cout << "无法打开名单" << endl;
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	string line;
	Relation relations[1000];
	int i = 0;
	char pwd[30];
	while (getline(fin, line)) {
		Relation rela;
		sscanf(line.c_str(), "%s:%s:%s", rela.student, rela.lesson, rela.teacher);
		relations[i++] = rela;
	}

	//查看作业并打分
	char buf[BLOCK_SIZE * 10];
	memset(buf, '\0', sizeof(buf));
	for (int j = 0; j < i; ++j) {
		//只看那门课和那个老师 bug
		// 
		//进入学生文件夹，查看学生文件
		char hw_path[310];
		sprintf(hw_path, "/home/%s/%s", relations[j].student, relations[j].lesson);
		cd_func(Cur_Dir_Addr, hw_path);
		cat(Cur_Dir_Addr, hwname);//输出学生的作业文件内容

		//教师根据学生作业打分( uname:score)
		double score = 0;
		char s_buf[30];
		printf("Please mark this assignment: ");
		scanf("%.2d", score);
		sprintf(s_buf, "%s:%.2d", relations[j].student, score);
		strcat(buf, s_buf);
	}

	//将成绩存入文档
	echo_func(Cur_Dir_Addr, score_path, ">", buf);
	return true;
}

