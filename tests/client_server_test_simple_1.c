#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */

int main(int argc, char **argv) {

    char *str1 = "AAAAAA!";
    char *str2 = "BBBBBB!";
    char *str3 = "AAAAAA!BBBBBB!";
    char *path = "/f1";
    char buffer[40];

    int f;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_mount(argv[1], argv[2]) == 0);

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str1, strlen(str1));
    assert(r == strlen(str1));

    r = tfs_write(f, str2, strlen(str2));
    assert(r == strlen(str2));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str3));

    buffer[r] = '\0';
    printf("buffer: %s\n", buffer);
    assert(strcmp(buffer, str3) == 0);

    sleep(10);

    assert(tfs_close(f) != -1);
    printf("after close 1\n");

    //assert(tfs_unmount() == 0);

    printf("Successful test.\n");

    return 0;
}