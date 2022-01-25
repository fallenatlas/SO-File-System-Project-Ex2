#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#define SIZE_CLIENT_PIPE_PATH 40

int session_id;
FILE *fserv;
FILE *fcli;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    size_t r;
    int s_id;
    char *ptr;
    char buf[2];
    char request[SIZE_CLIENT_PIPE_PATH + 2];

    // Create client pipe.
    unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0777) < 0) {
        exit(1);
    }

    // Write request to server pipe
    if ((fserv = fopen(server_pipe_path, "a")) == NULL) {
        exit(1);
    }

    request[0] = TFS_OP_CODE_MOUNT;
    strncpy(&request[1], client_pipe_path, SIZE_CLIENT_PIPE_PATH);
    r = fwrite(request, sizeof(char), SIZE_CLIENT_PIPE_PATH + 1, fserv);
    if (r <= 0) {
        return -1;
    }

    // Read response from the client pipe.
    if ((fcli = fopen(client_pipe_path, "r")) == NULL) {
        exit(1);
    }

    r = fread(buf, sizeof(char), 2, fcli);
    if (r <= 0) {
        return -1;
    }

    // Set the session_id.
    s_id = (int) strtol(buf, &ptr, 10);
    if (s_id == -1) {
        fclose(client_pipe_path);
        fclose(server_pipe_path);
        return -1;
    }
    session_id = s_id;

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    int r;
    int fhandle;
    size_t r1;
    char *ptr;
    char *buf;
    char request[50];
    char file_name[41];

    strncpy(file_name, name, SIZE_CLIENT_PIPE_PATH);
    r = snprintf(request, sizeof(request), "%c%d%40s%d", TFS_OP_CODE_OPEN, session_id, file_name, flags);
    if (r < 0) {
        return -1;
    }

    r1 = fwrite(request, sizeof(char), (size_t) r, fserv);
    if (r1 <= 0) {
        return -1;
    }

    r1 = fread(buf, sizeof(int), 1, fcli);
    if (r1 <= 0) {
        return -1;
    }

    fhandle = (int) strtol(buf, &ptr, 10);

    return fhandle;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    int r;
    size_t r1;
    char *ptr;
    char *buf;
    char request[10];

    r = snprintf(request, sizeof(request), "%c%d%d", TFS_OP_CODE_CLOSE, session_id, fhandle);
    if (r < 0) {
        return -1;
    }

    r1 = fwrite(request, sizeof(char), (size_t) r, fserv);
    if (r1 <= 0) {
        return -1;
    }

    r1 = fread(buf, sizeof(int), 1, fcli);
    if (r1 <= 0) {
        return -1;
    }

    r = (int) strtol(buf, &ptr, 10);

    return r;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    int r;
    size_t r1;
    char *ptr;
    char *buf;
    char request[4096];

    r = snprintf(request, sizeof(request), "%c%d%d%ld%s", TFS_OP_CODE_WRITE, session_id, fhandle, len, buffer);
    if (r < 0) {
        return -1;
    }

    r1 = fwrite(request, sizeof(char), (size_t) r, fserv);
    if (r1 <= 0) {
        return -1;
    }

    r1 = fread(buf, sizeof(ssize_t), 1, fcli);
    if (r1 <= 0) {
        return -1;
    }

    return (ssize_t) strtol(buf, &ptr, 10);
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
