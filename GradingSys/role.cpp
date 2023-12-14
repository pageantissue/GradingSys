#include<iostream>
#include<cstdio>
#include<cstring>
#include<fstream>
#include"function.h"
#include"os.h"
#include"role.h"
using namespace std;

bool add_users(char * namelist) {
	if (strcmp(Cur_Group_Name, "root") != 0) {
		printf("Only root could add users!\n");
		return false;
	}

	char new_buff[1024]; memset(new_buff, '\0', 1024);
	sprintf(new_buff, "../../../%s", namelist);
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	ifstream fin(new_buff);
	if (!fin.is_open()) {
		char ms[] = "Cannot open name list!\n";
		printf(ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	string line;
	Relation relations[100];
	int i = 0;
	char pwd[30];
	while (getline(fin, line)) {
		Relation rela;
		sscanf(line.c_str(), "%s %s %s", rela.student, rela.lesson, rela.teacher);
		relations[i++] = rela;
	}
	for (int j = 0; j < i; j++) {
		strcpy(pwd, relations[j].student);

		useradd(relations[j].student, pwd, "student");
		strcpy(pwd, relations[j].teacher);
		useradd(relations[j].teacher, pwd ,"teacher");
		//在其文件夹下创建对应课程文件夹

		char dir_path[100];
		sprintf(dir_path, "/home/%s/%s", relations[j].teacher, relations[j].lesson);
		mkdir_func(Cur_Dir_Addr, dir_path);
		sprintf(dir_path, "/home/%s/%s", relations[j].student, relations[j].lesson);
		mkdir_func(Cur_Dir_Addr, dir_path);
	}

	//恢复现场
	Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(Cur_Dir_Name, pro_cur_dir_name);
	printf("已批量添加账户\n");
	return true;
}

bool publish_task(char* lesson,char* filename) {
	if (strcmp(Cur_Group_Name, "teacher") != 0) {
		printf("Only teacher could publish tasks!\n");
		return false;
	}
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	char buf[BLOCK_SIZE * 10];
	string line;
	memset(buf, '\0', sizeof(buf));
	ifstream fin(filename);
	if (!fin.is_open()) {
		cout << "File Open Failure!" << endl;
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	while (getline(fin, line)) {
		strcat(buf, line.c_str());
	}

	//将file复制到虚拟OS中
	char* p = strstr(filename, ".");
	*p = '\0';
	char dir_path[100];
	sprintf(dir_path, "/home/%s/%s/%s_description", Cur_User_Name, lesson, filename);
	echo_func(Cur_Dir_Addr, dir_path, ">", buf); 

	return true;
}

bool judge_hw(char* namelist, char* lesson, char* hwname) {
	if (strcmp(Cur_Group_Name, "teacher") != 0) {
		printf("Only teacher could judge assignments!\n");
		return false;
	}
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	//新建本次作业评价文档( sname : mark)
	char* p = strstr(hwname, ".");
	*p = '\0';
	char score_path[310];
	sprintf(score_path, "/home/%s/%s/%s_score", Cur_Group_Name, lesson, hwname);
	touch_func(Cur_Dir_Addr, score_path, "");

	ifstream fin(namelist);
	if (!fin.is_open()) {
		cout << "File Open Failed!" << endl;
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

	char buf[BLOCK_SIZE * 10];
	memset(buf, '\0', sizeof(buf));
	for (int j = 0; j < i; ++j) {
		//是这个老师，而且是这门课
		if ((strcmp(relations[j].teacher, Cur_User_Name) == 0) && (strcmp(relations[j].lesson, lesson) == 0)) {
			//进入学生文件夹，查看学生文件
			char hw_path[310], s_buf[310];
			double score = 0;
			memset(hw_path, '\0', sizeof(hw_path));
			memset(s_buf, '\0', sizeof(s_buf));

			sprintf(hw_path, "/home/%s/%s", relations[j].student, relations[j].lesson);
			if (cd_func(Cur_Dir_Addr, hw_path)) {
				cat(Cur_Dir_Addr, hwname);//输出学生的作业文件内容
				printf("Please mark this assignment: ");//教师根据学生作业打分( uname:score)
				scanf("%.2d", score);
			}
			else {
				printf("%s doesn't hand out the homework!\n", relations[j].student);
			}

			sprintf(s_buf, "%s:%.2d", relations[j].student, score);
			strcat(buf, s_buf);
		}
	}

	echo_func(Cur_Dir_Addr, score_path, ">", buf);

	//恢复现场
	Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(Cur_Dir_Name, pro_cur_dir_name);
	return true;
}

bool check_hw_content(char* teacher_name, char* lesson, char* hwname) {
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);
	gotoRoot();
	cd(Cur_Dir_Addr, "home");
	bool f = cd(Cur_Dir_Addr, teacher_name);
	if (!f)
	{
		char ms[] = "Teacher does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	f = cd(Cur_Dir_Addr, lesson);
	if (!f)
	{
		char ms[] = "Lesson does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	f = cat(Cur_Dir_Addr, hwname);
	if (!f)
	{
		char ms[] = "Specific homework does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(Cur_Dir_Name, pro_cur_dir_name);
	return true;
}

bool check_hw_score(char* teacher_name, char* lesson, char* hwname) {
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	gotoRoot();
	cd(Cur_Dir_Addr, "home");
	bool f = cd(Cur_Dir_Addr, teacher_name);
	if (!f)
	{
		char ms[] = "Teacher does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	f = cd(Cur_Dir_Addr, lesson);
	if (!f)
	{
		char ms[] = "Lesson does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	char hw_score[FILE_NAME_MAX_SIZE]; 
	memset(hw_score, '\0', FILE_NAME_MAX_SIZE);
	sprintf(hw_score, "%s_score", hwname);
	f = cd(Cur_Dir_Addr, hw_score);
	if (!f)
	{
		char ms[] = "Specific homework does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(Cur_Dir_Name, pro_cur_dir_name);
	return true;
}

bool submit_assignment(char* student_name, char* lesson, char* filename)
{
	if (strcmp(Cur_Group_Name, "student") != 0) {
		char ms[] = "Only student could submit and upload his homework!\n";
		printf(ms);
		return false;
	}
	int pro_cur_dir_addr = Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, Cur_Dir_Name);

	gotoRoot();
	cd(Cur_Dir_Addr, "home");

	bool f = cd(Cur_Dir_Addr, student_name);
	if (!f)
	{
		char ms[] = "This student does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}

	f = cd(Cur_Dir_Addr, lesson);
	if (!f)
	{
		char ms[] = "Lesson does not exist!\n";
		printf("%s\n", ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}

	char buf[BLOCK_SIZE * 10];
	string line;
	memset(buf, '\0', sizeof(buf));
	ifstream fin(filename);
	if (!fin.is_open()) {
		char ms[] = "Cannot open file!\n";
		printf(ms);
		Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	while (getline(fin, line)) {
		strcat(buf, line.c_str());
	}

	char dir_path[100];
	sprintf(dir_path, "/home/%s/%s/%s_description", Cur_User_Name, lesson, filename);
	echo_func(Cur_Dir_Addr, dir_path, ">", buf);

	Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(Cur_Dir_Name, pro_cur_dir_name);
	return true;
}
