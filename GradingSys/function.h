#pragma once
#include<iostream>
#include"os.h"
#include"role.h"

void help();
void cmd(char cmd[]);


bool cd_func(int CurAddr, char* str);
bool mkdir_func(int CurAddr, char* str);
bool rm_func(int CurAddr, char* str, char* s_type);
bool touch_func(int CurAddr, char* str, char* buf);
bool echo_func(int CurAddr, char* str, char* s_type, char* buf);
bool chmod_func(int CurAddr, char* pmode, char* str);
bool chown_func(int CurAddr, char* u_g, char* str);
bool passwd_func(char* username);