#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int p1[2], p2[2];
    char buf[] = {'a'};
    pipe(p1);
    pipe(p2);

    int pid = fork();
    if (pid == 0) {
        close(p1[1]);
        close(p2[0]);
        read(p1[0], buf, sizeof(buf));
        fprintf(2, "%d: received ping\n", getpid());
        write(p2[1], buf, sizeof(buf));
        exit(0);
    }
    else {
        close(p1[0]);
        close(p2[1]);
        write(p1[1], buf, sizeof(buf));
        read(p2[0], buf, sizeof(buf));
        fprintf(2, "%d: received pong\n", getpid());
        exit(0);
    }

    exit(0);
}