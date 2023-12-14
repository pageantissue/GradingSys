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


bool add_users(char*);
bool publish_task(char* lesson, char* filename);
bool judge_hw(char* namelist, char* lesson, char* hwname);
bool check_hw_content(char* teacher_name, char* lesson, char* hwname);
bool check_hw_score(char* teacher_name, char* lesson, char* hwname);
bool submit_assignment(char* student_name, char* lesson, char* filename);