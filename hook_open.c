#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdarg.h>
#include <unistd.h>
#include <libio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"

typedef struct _IO_FILE FILE;

//类open函数指针
typedef int (*OpenFunction)(const char* fn, int flags, ...);
//类fopen函数指针
typedef FILE* (*FopenFunction)(char* fn, char *mode);
//自定义open函数指针
OpenFunction real_open;
//自定义fopen函数指针
FopenFunction real_fopen;

//判断文件是否在监控目录中，且其中是否含有敏感词
int file_contain_string(const char *fn) {
    //监控文件中的敏感词和路径
    char dest_string[1024], dest_path[1024];
    //当前文件的绝对路径
    char real_path[1024];
    int i = 0,j = 0;
    //当前文件是否处在监控中
    int file_flag = 1, string_flag = 0;

    //获取监控文件的指针
    FILE *fp = ((FILE* (*)(char*, char*))(dlsym(RTLD_NEXT, "fopen")))(MONITOR_CONF, "r");
    if (!fp) return 0;
    
    //获取libc中fgets的地址
    char* (*fgets_ptr)(char*, int, FILE*)=dlsym(RTLD_NEXT, "fgets");
    char s[4096];
    //初始化敏感词和监控路径
    while((fgets_ptr(s, 4096, fp))!=NULL) {
	if (strstr(s, "string=")) {
	    strcpy(dest_string, strstr(s, "string=")+7);
	    dest_string[strlen(dest_string)-1]=0;
	}
	if (strstr(s, "path=")) {
	    strcpy(dest_path, strstr(s, "path=")+5);
	    dest_path[strlen(dest_path)-1]=0;
	}
    }

    //获取文件绝对路径
    realpath(fn, real_path);
    //判断是否在监控路径
    while (i<strlen(dest_path) && j<strlen(real_path)) {
	char tmp1[256] = "",tmp2[256] = "";
	int k = 0;
	while (dest_path[i] && dest_path[i]!='/') tmp1[k++]=dest_path[i++];
	k = 0;
	while (real_path[j] && real_path[j]!='/') tmp2[k++]=real_path[j++];
	if (strcmp(tmp1, tmp2) != 0) {
	    file_flag = 0;
	    break;
	}
	i++;
	j++;
    }
    if (!file_flag || i < strlen(dest_path)) return 0;

    //获取当前文件的指针
    FILE *fp1 = ((FILE* (*)(char*, char*))(dlsym(RTLD_NEXT, "fopen")))(real_path, "r"); 
    if (!fp1) return 0;
    //判断文件中是否含有敏感词
    while((fgets_ptr(s, 4096, fp1))!=NULL) {
	if (strstr(s, dest_string)) string_flag = 1;
    }
    if (!string_flag) return 0;
    //关闭文件指针
    ((int (*)(FILE*))(dlsym(RTLD_NEXT, "fclose")))(fp1); 

    //关闭文件指针
    ((int (*)(FILE*))(dlsym(RTLD_NEXT, "fclose")))(fp); 

    return 1;
}

//自定义open函数
int open(const char* fn, int flags, ...) {
    int r = -1;
    va_list args;

    //判断文件是否处在监控中
    if (file_contain_string(fn)) {
        char output[1024];
        ((int (*)(char*, const char*, ...))(dlsym(RTLD_NEXT, "sprintf")))(output, "含有敏感词，禁止访问\n", fn);
        write(1, output, strlen(output));
	return -1;
    }

    //执行open操作
    va_start(args, flags);
    if (!real_open) {
        void** pointer;

        pointer = (void**)&real_open;
        *pointer = dlsym(RTLD_NEXT, "open");
    }
    r = real_open(fn, flags, args);
    va_end(args);
    
    //char output[1024];
    //((int (*)(char*, const char*, ...))(dlsym(RTLD_NEXT, "sprintf")))(output, "try open %s\n", fn);
    //write(1, output, strlen(output));
   
    return r;
}

//自定义fopen函数
FILE *fopen(char *fn, char *mode) {
    FILE * fp = NULL;

    //判断文件是否处在监控中
    if (file_contain_string(fn)) {
        char output[1024];
        ((int (*)(char*, const char*, ...))(dlsym(RTLD_NEXT, "sprintf")))(output, "含有敏感词，禁止访问\n", fn);
        write(1, output, strlen(output));
	return NULL;
    }

    //执行fopen操作
    if (!real_fopen) {
        void **pointer;

        pointer = (void**)&real_fopen;
        *pointer = dlsym(RTLD_NEXT, "fopen");
    }
    fp = real_fopen(fn, mode);

    //char output[1024];
    //((int (*)(char*, const char*, ...))(dlsym(RTLD_NEXT, "sprintf")))(output, "try fopen %s\n", fn);
    //write(1, output, strlen(output));

    return fp;
}
