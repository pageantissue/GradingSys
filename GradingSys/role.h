#pragma once
#include<iostream>
#include<cstdio>
#include<ctime>
#include<cstring>
#include"function.h"
#include"os.h"

struct Relation {
	char student[30];
	char lesson[30];
	char teacher[30];
};

bool check_hw_content(Client&, char* lesson, char* hwname);
bool check_hw_score(Client&, char* lesson, char* hwname);
bool submit_assignment(Client&, char* student_name, char* lesson, char* filename);
bool add_users(Client&, char* namelist);			//root：批量创建教师及学生用户
bool publish_task(Client&, char* lesson, char* filename);	//teacher:发布本次作业任务
bool judge_hw(Client&, char* namelist, char* lesson, char* hwname);//teacher:评价本次作业
