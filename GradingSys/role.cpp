#include<iostream>
#include<cstdio>
#include<cstring>
#include<fstream>
#include"function.h"
#include"server.h"
#include"os.h"
#include"role.h"
using namespace std;

bool add_users(Client& client, char * namelist) {
	if (strcmp(client.Cur_Group_Name, "root") != 0) {
		printf("Only root could add users!\n");
		return false;
	}

	char new_buff[1024]; memset(new_buff, '\0', 1024);
	//sprintf(new_buff, "../../../%s", namelist);
    sprintf(new_buff, "/Users/sprungissue/CLionProjects/GradingSys/GradingSys/%s", namelist);
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);

	ifstream fin(new_buff);
	if (!fin.is_open()) {
		char ms[] = "Cannot open name list!\n";
		printf(ms);
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
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

		useradd(client, relations[j].student, pwd, "student");
		strcpy(pwd, relations[j].teacher);
		useradd(client, relations[j].teacher, pwd ,"teacher");
		//在其文件夹下创建对应课程文件夹

		char dir_path[100];
		sprintf(dir_path, "/home/%s/%s", relations[j].teacher, relations[j].lesson);
		mkdir_func(client, client.Cur_Dir_Addr, dir_path);
		sprintf(dir_path, "/home/%s/%s", relations[j].student, relations[j].lesson);
		mkdir_func(client, client.Cur_Dir_Addr, dir_path);
	}

	//恢复现场
	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
	printf("已批量添加账户\n");
	return true;
}

bool publish_task(Client& client, char* lesson, char* filename) {//ok
	if (strcmp(client.Cur_Group_Name, "teacher") != 0) {
		printf("Only teacher could publish tasks!\n");
		return false;
	}
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);

	char buf[BLOCK_SIZE * 10];
	string line;
	memset(buf, '\0', sizeof(buf));
    char new_buff[1024];
    memset(new_buff, '\0', 1024);
	//sprintf(new_buff, "../../../%s.txt", filename);
    sprintf(new_buff, "/Users/sprungissue/CLionProjects/GradingSys/GradingSys/%s.txt", filename);
	ifstream fin(new_buff);
	if (!fin.is_open()) {
		cout << "File Open Failure!" << endl;
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	while (getline(fin, line)) {
		strcat(buf, line.c_str());
	}

	//将file复制到虚拟OS中
//	char* p = strstr(filename, ".");
//	*p = '\0';
	char dir_path[100];
	sprintf(dir_path, "/home/%s/%s/%s_description", client.Cur_User_Name, lesson, filename);
	echo_func(client, client.Cur_Dir_Addr, dir_path, ">", buf);

    //恢复现场
    client.Cur_Dir_Addr = pro_cur_dir_addr;
    strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
    char ms[] = "Successfully published task!\n";

	return true;
}

bool judge_hw(Client& client, char* namelist, char* lesson, char* hwname) {
	if (strcmp(client.Cur_Group_Name, "teacher") != 0) {
		printf("Only teacher could judge assignments!\n");
		return false;
	}
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);

	//新建本次作业评价文档( sname : mark)
//	char* p = strstr(hwname, ".");
//	*p = '\0';
    char new_buff[100];
    memset(new_buff, '\0', 100);
	//sprintf(new_buff, "../../../%s", namelist);
    sprintf(new_buff, "/Users/sprungissue/CLionProjects/GradingSys/GradingSys/%s", namelist);

	ifstream fin(new_buff);
	if (!fin.is_open()) {
		cout << "File Open Failed!" << endl;
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	string line;
	Relation relations[1000];
	int i = 0;
	char pwd[30];
	while (getline(fin, line)) {
		Relation rela;
		sscanf(line.c_str(), "%s %s %s", rela.student, rela.lesson, rela.teacher);
		relations[i++] = rela;
	}

	for (int j = 0; j < i; ++j) {
        //是这个老师，而且是这门课
        if ((strcmp(relations[j].teacher, client.Cur_User_Name) == 0) && (strcmp(relations[j].lesson, lesson) == 0))
        {
            char buf[BLOCK_SIZE * 10];
            memset(buf, '\0', sizeof(buf));
            //进入学生文件夹，查看学生文件
            char hw_path[310];
            double score = 0;
            memset(hw_path, '\0', sizeof(hw_path));

            sprintf(hw_path, "/home/%s/%s", relations[j].student, relations[j].lesson);
            if (cd_func(client, client.Cur_Dir_Addr, hw_path))
            {
                char myname[100];
                memset(myname, '\0', 100);
                sprintf(myname, "%s_%s", hwname, relations[j].student);
                if (cat(client, client.Cur_Dir_Addr, myname)) {//输出学生的作业文件内容
                    //如果找到了作业，打分
                    printf("Please mark this assignment: ");//教师根据学生作业打分( uname:score)
                    scanf("%lf", &score);
                } else {
                    printf("%s doesn't hand out the homework!\n", relations[j].student);
                }
                sprintf(buf, "%s: %.2f\n", relations[j].student, score);
                char save_path[100];
                memset(save_path, '\0', 100);
                sprintf(save_path, "/home/%s/%s/%s_score", client.Cur_User_Name, lesson, myname);
                echo_func(client, client.Cur_Dir_Addr, save_path, ">",  buf);
            }
        }
    }

	//恢复现场
	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
	return true;
}

bool check_hw_content(Client& client, char* lesson, char* hwname)
{//ok
    int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);

    gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "home");
    char new_buff[1024];
    memset(new_buff, '\0', 1024);
    sprintf(new_buff, "/Users/sprungissue/CLionProjects/GradingSys/GradingSys/%s", STUDENT_COURSE_LIST);
    ifstream fin(new_buff);
    if (!fin.is_open()) {
        cout << "File Open Failed!" << endl;
        client.Cur_Dir_Addr = pro_cur_dir_addr;
        strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
        return false;
    }
    string line;
    Relation relations[1000];
    int i = 0;
    char pwd[30];
    while (getline(fin, line)) {
        Relation rela;
        sscanf(line.c_str(), "%s %s %s", rela.student, rela.lesson, rela.teacher);
        relations[i++] = rela;
    }
    for (int j = 0; j < i; ++j)
    {
        //是这个student，而且是这门课
        if ((strcmp(relations[j].student, client.Cur_User_Name) == 0) && (strcmp(relations[j].lesson, lesson) == 0))
        {
            //进入老师文件夹，查看作业描述文件
            char hw_path[310], s_buf[310];
            memset(hw_path, '\0', sizeof(hw_path));
            memset(s_buf, '\0', sizeof(s_buf));

            sprintf(hw_path, "/home/%s/%s", relations[j].teacher, relations[j].lesson);
            if (cd_func(client, client.Cur_Dir_Addr, hw_path))
            {
                char myname[100];
                memset(myname, '\0', 100);
                sprintf(myname, "%s_description", hwname);
                if (cat(client, client.Cur_Dir_Addr, myname)) {
                    //如果找到了作业,cat
                    client.Cur_Dir_Addr = pro_cur_dir_addr;
                    strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
                    return true;
                } else {
                    printf("Teacher has not published any homework yet!\n");
                    client.Cur_Dir_Addr = pro_cur_dir_addr;
                    strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
                    return false;
                }
            }
        }
    }
}

bool check_hw_score(Client& client, char* lesson, char* hwname)
{
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);

	gotoRoot(client);
    cd(client, client.Cur_Dir_Addr, "home");
    char new_buff[1024];
    memset(new_buff, '\0', 1024);
    sprintf(new_buff, "/Users/sprungissue/CLionProjects/GradingSys/GradingSys/%s", STUDENT_COURSE_LIST);
    ifstream fin(new_buff);
    if (!fin.is_open()) {
        cout << "File Open Failed!" << endl;
        client.Cur_Dir_Addr = pro_cur_dir_addr;
        strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
        return false;
    }
    string line;
    Relation relations[1000];
    int i = 0;
    char pwd[30];
    while (getline(fin, line)) {
        Relation rela;
        sscanf(line.c_str(), "%s %s %s", rela.student, rela.lesson, rela.teacher);
        relations[i++] = rela;
    }
    for (int j = 0; j < i; ++j)
    {
        //是这个student，而且是这门课
        if ((strcmp(relations[j].student, client.Cur_User_Name) == 0) && (strcmp(relations[j].lesson, lesson) == 0))
        {
            //进入老师文件夹，查看作业成绩文件
            char hw_path[310];
            memset(hw_path, '\0', sizeof(hw_path));

            sprintf(hw_path, "/home/%s/%s", relations[j].teacher, relations[j].lesson);
            if (cd_func(client, client.Cur_Dir_Addr, hw_path))
            {
                //HW1_JeffD : _score
                char myname[100];
                memset(myname, '\0', 100);
                sprintf(myname, "%s_%s_score", hwname, client.Cur_User_Name);
                if (cat(client, client.Cur_Dir_Addr, myname)) {
                    //如果找到了作业,cat
                    client.Cur_Dir_Addr = pro_cur_dir_addr;
                    strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
                    return true;
                } else {
                    printf("Teacher has not graded your assignment yet!\n");
                    client.Cur_Dir_Addr = pro_cur_dir_addr;
                    strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
                    return false;
                }
            }
        }
    }
}

bool submit_assignment(Client& client, char* student_name, char* lesson, char* filename)
{
	if (strcmp(client.Cur_Group_Name, "student") != 0) {
		char ms[] = "Only student could submit and upload his homework!\n";
		printf(ms);
		return false;
	}
	int pro_cur_dir_addr = client.Cur_Dir_Addr;
	char pro_cur_dir_name[310];
	memset(pro_cur_dir_name, '\0', sizeof(pro_cur_dir_name));
	strcpy(pro_cur_dir_name, client.Cur_Dir_Name);

	gotoRoot(client);
	cd(client, client.Cur_Dir_Addr, "home");

	bool f = cd(client, client.Cur_Dir_Addr, student_name);
	if (!f)
	{
		char ms[] = "This student does not exist!\n";
		printf("%s\n", ms);
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}

	f = cd(client, client.Cur_Dir_Addr, lesson);
	if (!f)
	{
		char ms[] = "Lesson does not exist!\n";
		printf("%s\n", ms);
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}

	char buf[BLOCK_SIZE * 10];
	string line;
	memset(buf, '\0', sizeof(buf));
    char new_buff[100];
    memset(new_buff, '\0', 100);
	//sprintf(new_buff, "../../../%s.txt", filename);
    sprintf(new_buff, "/Users/sprungissue/CLionProjects/GradingSys/GradingSys/%s.txt", filename);
	ifstream fin(new_buff);
	if (!fin.is_open()) {
		char ms[] = "Cannot open file!\n";
		printf(ms);
		client.Cur_Dir_Addr = pro_cur_dir_addr;
		strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
		return false;
	}
	while (getline(fin, line)) {
		strcat(buf, line.c_str());
	}

	char dir_path[100];
	sprintf(dir_path, "/home/%s/%s/%s_%s", client.Cur_User_Name, lesson, filename, client.Cur_User_Name);
	echo_func(client, client.Cur_Dir_Addr, dir_path, ">", buf);

	client.Cur_Dir_Addr = pro_cur_dir_addr;
	strcpy(client.Cur_Dir_Name, pro_cur_dir_name);
    char ms[] = "Successfully submit your HW!\n";
	send(client.client_sock, ms, strlen(ms), 0);
	return true;
}
