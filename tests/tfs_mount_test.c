#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    char *path = "/f1";
    int f;

    assert(tfs_mount(argv[1], argv[2]) == 0);
    printf("successful mount\n");
    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);
    printf("successful open\n");
    assert(tfs_unmount() == 0);
    printf("successful unmount\n");
    printf("Successful test.\n");

    return 0;
}