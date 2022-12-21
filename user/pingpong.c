#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    // 0 为标准输入 1 为标准输出
    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);
    if (fork() == 0) {
        char buf;
        if ((read(p1[0], &buf, 1)) > 0) {
            printf("%d: received ping\n", getpid());
            write(p2[1], "a", 1);
        }
    } else {
        char buf;
        write(p1[1], "a", 1);
        if ((read(p2[0], &buf, 1)) > 0) {
            printf("%d: received pong\n", getpid());
            wait(0);
        }
    }
    exit(0);
}