#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

void run(char *program, char **args) {
    if (fork() == 0) {
        exec(program, args);
        exit(0);
    }
    return;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(2, "Usage xargs cmd...");
        exit(0);
    }
    char buf[2048];
    char *p = buf, *last_p = buf;
    char *argsbuf[128];
    char **args = argsbuf;
    for (int i = 1; i < argc; i++) {
        *args = argv[i];
        args++;
    }
    char **pa = args;
    while (read(0, p, 1)) {
        if (*p == ' ' || *p == '\n') {
            *p = '\0';
            *(pa++) = last_p;
            last_p = p + 1;

            if (*p == '\n') {
                *pa = 0;
                run(argv[1], argsbuf);
                pa = args;
            }
        }
        p++;
    }
    if (pa != args) {
        *p = '\0';
        *(pa++) = last_p;
        *pa = 0;
        run(argv[1], argsbuf);
    }
    while (wait(0) != -1) {
    };
    exit(0);
}