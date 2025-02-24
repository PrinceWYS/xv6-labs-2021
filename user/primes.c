#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int leftPipe[2]) {
    int x;
    read(leftPipe[0], &x, sizeof(x));

    if (x == -1) {
        // 读取完毕
        exit(0);
    }

    fprintf(2, "prime %d\n", x);

    int rightPipe[2];
    pipe(rightPipe);

    if (fork() == 0) {
        close(leftPipe[0]);
        close(rightPipe[1]);
        prime(rightPipe);
    }
    else {
        close(rightPipe[0]);

        int buf;
        while (read(leftPipe[0], &buf, sizeof(buf)) && buf != -1) {
            if (buf % x != 0) {
                write(rightPipe[1], &buf, sizeof(buf));
            }
        }
        buf = -1;
        write(rightPipe[1], &buf, sizeof(buf));
        wait(0);
        exit(0);
    }
}

int main() {
    int p[2];
    pipe(p);

    if (fork() == 0) {
        // 子进程
        close(p[1]);    // 关闭写口
        prime(p);
        exit(0);
    }
    else {
        // 原进程
        close(p[0]);    // 关闭读口
        int i;
        for (i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }
        i = -1;
        write(p[1], &i, sizeof(i));
    }

    wait(0);    // 等待子进程完成
    exit(0);
}