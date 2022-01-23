#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#define SIZE_CLIENT_PIPE_PATH 40

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    FILE *fserv;
    FILE *fcli;
    char buf[2];
    char request[SIZE_CLIENT_PIPE_PATH + 2];

    if ((fserv = fopen(server_pipe_path, "a")) == NULL) {
        exit(1);
    }

    request[0] = TFS_OP_CODE_MOUNT;
    strncpy(&request[1], client_pipe_path, SIZE_CLIENT_PIPE_PATH);

    fwrite(request, sizeof(char), SIZE_CLIENT_PIPE_PATH + 1, fserv);

    if ((fcli = fopen(client_pipe_path, "a")) == NULL) {
        exit(1);
    }

    fread(buf, sizeof(char), 2, fcli);
    // set id to the return value if it is not -1;

    return -1;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
