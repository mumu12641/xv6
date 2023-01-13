#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char* path, char* target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        case T_DEVICE:
        case T_FILE:
            if (strcmp(path + strlen(path) - strlen(target), target) == 0) {
                printf("%s\n", path);
            }
            break;
        case T_DIR:
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                // Don't recurse into "." and "..".
                if (strcmp(buf + strlen(buf) - 1, ".") != 0 &&
                    strcmp(buf + strlen(buf) - 2, "..") != 0) {
                    find(buf, target);
                }
            }
            break;
    }
    close(fd);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(2, "Usage: find path target\n");
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}