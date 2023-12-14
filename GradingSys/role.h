#pragma once
#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string.h>
#include"function.h"
#include"os.h"

struct Relation {
	char student[30];
	char lesson[30];
	char teacher[30];
};


bool check_hw_content(char* teacher_name, char* lesson, char* hwname);
bool check_hw_score(char* teacher_name, char* lesson, char* hwname);
bool submit_assignment(char* student_name, char* lesson, char* filename);
bool add_users(char* namelist);			//root：批量创建教师及学生用户
bool publish_task(char* lesson, char* filename);	//teacher:发布本次作业任务
bool judge_hw(char* namelist, char* lesson, char* hwname);//teacher:评价本次作业
