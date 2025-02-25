#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

void run(char *program, char **params) {
    if (fork() == 0) {
        exec(program, params);
        exit(0);
    }
    return;
}

int main(int argc, char *argv[]) {
    char buf[MAXARG] = {0};
    char *p = buf, *last_p = buf;   // 当前参数的指针, 上一个参数指针
    char *xargv[MAXARG] = {0};            // 所有参数列表
    char **args = xargv;            // 第一个从 stdin 读入的参数

    for (int i = 1; i < argc; i++) {
        *args = argv[i];
        args++;
    }

    char **pa = args;   // 开始读入参数
    while (read(0, p, 1) != 0) {
        if (*p == ' ' || *p == '\n') {
            int isLine = (*p == '\n');  // 是否是一行结束

            *p = '\0';
            *(pa++) = last_p;
            last_p = p + 1;

            if (isLine) {
                *pa = 0;
                run(argv[1], xargv);    // 执行这一行
                pa = args;              // 准备执行下一行
            }            
        }
        p++;
    }

    if (pa != args) {   // 最后一行不是空行
        *p = '\0';
        *(pa++) = last_p;
        *pa = 0;
        run(argv[1], xargv);
    }

    while (wait(0) != -1)
    {}

    exit(0);
}