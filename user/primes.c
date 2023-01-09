#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// p = get a number from left neighbor
// print p
// loop:
// n = get a number from left neighbor
// if (p does not divide n)
//     send n to right neighbor

void sieve(int p[2]) {
    int prime;
    read(p[0], &prime, sizeof(prime));

    printf("prime %d\n", prime);
    if (prime == 31) {
        exit(0);
    }

    int _p[2];
    pipe(_p);
    if (fork() == 0) {
        close(_p[1]);
        close(p[0]);
        sieve(_p);
    } else {
        close(_p[0]);
        int number;
        while ((read(p[0], &number, sizeof(number))) > 0) {
            if (number % prime != 0) {
                write(_p[1], &number, sizeof(number));
            }
        }
        // Hint: read returns zero when the write-side of a pipe is closed.
        close(_p[1]);
        wait(0);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(p[1]);
        sieve(p);
        exit(0);
    } else {
        close(p[0]);
        int i;
        for (i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
    }
    wait(0);
    exit(0);
}