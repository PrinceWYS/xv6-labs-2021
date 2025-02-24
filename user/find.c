#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *curPath, char *filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(curPath, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", curPath);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", curPath);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        // 匹配到文件
        if (strcmp(curPath + strlen(curPath) - strlen(filename), filename) == 0) {
            printf("%s\n", curPath);
        }
        break;
    
    case T_DIR:
        // 匹配到文件夹, 递归查询
        if (strlen(curPath) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, curPath);
        p = buf + strlen(buf);
        *p++ = '/';

        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0) {
                continue;
            }
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            if (strcmp(buf + strlen(buf) - 2, "/.") && 
                strcmp(buf + strlen(buf) - 3, "/..")) {
                find(buf, filename);
            }
        }

        break;
    
    default:
        break;
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Error usage\n");
        exit(1);
    }

    char file[512];
    file[0] = '/';
    strcpy(file + 1, argv[2]);


    find(argv[1], file);

    exit(0);
}