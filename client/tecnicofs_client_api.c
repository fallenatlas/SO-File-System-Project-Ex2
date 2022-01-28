#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h> // -> unlink
#include <sys/stat.h> // -> mkfifo
#include <fcntl.h> // -> 0_RDONLY
#include <errno.h>

#define SIZE_CLIENT_PIPE_PATH 40
#define SIZE_FILE_NAME_PATH 40

int session_id;
int fserv;
int fcli;
char const *client_pipe_name;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    /*
    //int r;
    ssize_t r1;
    int s_id;
    char *ptr;
    char buf[4];
    //char client_pipe[41];
    char request[SIZE_CLIENT_PIPE_PATH + 2];

    // Create client pipe.
    unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0640) < 0) {
        exit(1);
    }
    client_pipe_name = client_pipe_path;

    // Write request to server pipe
    if ((fserv = open(server_pipe_path, O_WRONLY)) == -1) {
        exit(1);
    }

    request[0] = '1';
    strncpy(&request[1], client_pipe_path, SIZE_CLIENT_PIPE_PATH);
    printf("request: %s\n", request);
    printf("before writing\n");
    r1 = write(fserv, request, sizeof(char)*(SIZE_CLIENT_PIPE_PATH+1));
    //printf("after writing\n");
    printf("written: %ld\n", r1);
    if (r1 <= 0) {
        return -1;
    }

    printf("after written\n");
    // Read response from the client pipe.
    if ((fcli = open(client_pipe_path, O_RDONLY)) == -1) {
        exit(1);
    }

    printf("after open\n");
    r1 = read(fcli, buf, sizeof(int));
    if (r1 <= 0) {
        return -1;
    }

    printf("before convert: %s\n", buf);
    // Set the session_id.
    s_id = (int) strtol(buf, &ptr, 10);
    if (s_id == -1) {
        close(fserv);
        close(fcli);
        unlink(client_pipe_path);
        return -1;
    }
    session_id = s_id;
    printf("Session id: %d\n", session_id);
    return 0;
    */


    /* NEW IMPLEMENTATION */
    int s_id;
    char op_code = '1';
    char request[42];
    memset(request, '\0', sizeof(request));

    // Create client pipe.
    unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0640) < 0) {
        exit(1);
    }
    client_pipe_name = client_pipe_path;

    // Write request to server pipe
    if ((fserv = open(server_pipe_path, O_WRONLY)) == -1) {
        exit(1);
    }

    request[0] = op_code;
    strncpy(&request[1], client_pipe_path, SIZE_CLIENT_PIPE_PATH*sizeof(char));

    if (write(fserv, request, sizeof(char)*(SIZE_CLIENT_PIPE_PATH+1)) == -1) {
        return -1;
    }

    // Read response from the client pipe.
    if ((fcli = open(client_pipe_path, O_RDONLY)) == -1) {
        exit(1);
    }

    if (read(fcli, &s_id, sizeof(int)) == -1) {
        return -1;
    }
    
    printf("s_id: %d\n", s_id);
    // Set the session_id.
    if (s_id == -1) {
        close(fserv);
        close(fcli);
        unlink(client_pipe_path);
        return -1;
    }

    session_id = s_id;
    printf("Session id: %d\n", session_id);
    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    /*
    int r;
    ssize_t r1;
    char *ptr;
    char buf[5];
    char request[50];

    r = snprintf(request, sizeof(request), "%c%d", '2', session_id);
    if (r < 0) {
        return -1;
    }

    r1 = write(fserv, request, sizeof(char)*(size_t)r);
    if (r1 <= 0) {
        return -1;
    }

    r1 = read(fcli, buf, sizeof(int));
    printf("unmount r1: %ld", r1);
    if (r1 <= 0) {
        return -1;
    }

    printf("before convert unmount: %s\n", buf);
    r = (int) strtol(buf, &ptr, 10);
    if (r == -1) {
        return -1;
    }

    close(fserv);
    close(fcli);
    unlink(client_pipe_name);
    return 0;
    */

    /* NEW IMPLEMENTATION */
    int r;
    char op_code = '2';
    char request[5];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+1, &session_id, sizeof(int));

    if (write(fserv, request, sizeof(char)+sizeof(int)) == -1) {
        return -1;
    }

    if (read(fcli, &r, sizeof(int)) == -1) {
        return -1;
    }

    printf("return: %d\n", r);
    if (r == -1) {
        return r;
    }

    close(fserv);
    close(fcli);
    unlink(client_pipe_name);
    return r;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    /*
    int r;
    int fhandle;
    ssize_t r1;
    char *ptr;
    char buf[20];
    char request[50];
    char file_name[41];

    strncpy(file_name, name, SIZE_CLIENT_PIPE_PATH);
    r = snprintf(request, sizeof(request), "%c%d%40s%d", '3', session_id, file_name, flags);
    if (r < 0) {
        return -1;
    }

    r1 = write(fserv, request, sizeof(char)*(size_t)r);
    if (r1 <= 0) {
        return -1;
    }

    r1 = read(fcli, buf, sizeof(int));
    if (r1 <= 0) {
        return -1;
    }

    fhandle = (int) strtol(buf, &ptr, 10);

    return fhandle;
    */
    /* NEW IMPLEMENTATION */
    int fhandle;
    char op_code = '3';
    char request[50];
    memset(request, '\0', sizeof(request));
    
    //request[0] = op_code;
    memcpy(request, &op_code, sizeof(char));
    memcpy(request+1, &session_id, sizeof(int));
    strncpy(request+1+sizeof(int), name, SIZE_FILE_NAME_PATH*sizeof(char));
    memcpy(request+1+sizeof(int)+(SIZE_FILE_NAME_PATH*sizeof(char)), &flags, sizeof(int));

    //printf("open request: %s\n", request);
    if (write(fserv, request, sizeof(char)+sizeof(int)+(SIZE_FILE_NAME_PATH*sizeof(char))+sizeof(int)) == -1) {
        return -1;
    }

    if (read(fcli, &fhandle, sizeof(int)) == -1) {
        return -1;
    }

    printf("fhandle: %d\n", fhandle);
    return fhandle;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    /*
    int r;
    ssize_t r1;
    char *ptr;
    char buf[20];
    char request[10];

    r = snprintf(request, sizeof(request), "%c%d%d", '4', session_id, fhandle);
    if (r < 0) {
        return -1;
    }

    r1 = write(fserv, request, sizeof(char)*(size_t)r);
    if (r1 <= 0) {
        return -1;
    }

    r1 = read(fcli, buf, sizeof(int));
    if (r1 <= 0) {
        return -1;
    }

    r = (int) strtol(buf, &ptr, 10);

    return r;
    */
    /* NEW IMPLEMENTATION */
    int r;
    char op_code = '4';
    char request[10];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+1, &session_id, sizeof(int));
    memcpy(request+1+sizeof(int), &fhandle, sizeof(int));

    if (write(fserv, request, sizeof(char)+sizeof(int)+sizeof(int)) == -1) {
        return -1;
    }

    if (read(fcli, &r, sizeof(int)) == -1) {
        return -1;
    }

    printf("return close: %d\n", r);
    return r;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    /*
    int r;
    ssize_t r1;
    char *ptr;
    char buf[20];
    char request[4096];

    r = snprintf(request, sizeof(request), "%c%d%d%ld%p", '5', session_id, fhandle, len, buffer);
    if (r < 0) {
        return -1;
    }

    r1 = write(fserv, request, sizeof(char)*(size_t)r);
    if (r1 <= 0) {
        return -1;
    }

    r1 = read(fcli, buf, sizeof(ssize_t));
    if (r1 <= 0) {
        return -1;
    }

    return (ssize_t) strtol(buf, &ptr, 10);
    */
    /* NEW IMPLEMENTATION */
    ssize_t r;
    char op_code = '5';
    //char request[4096];
    char *request = (char*) malloc(sizeof(char)+sizeof(int)+sizeof(int)+sizeof(size_t)+len*sizeof(char));

    request[0] = op_code;
    memcpy(request+1, &session_id, sizeof(int));
    memcpy(request+1+sizeof(int), &fhandle, sizeof(int));
    memcpy(request+1+sizeof(int)+sizeof(int), &len, sizeof(size_t));
    strncpy(request+1+sizeof(int)+sizeof(int)+sizeof(size_t), buffer, len*sizeof(char));

    //printf("write request: %s\n", request);
    if (write(fserv, request, 1+sizeof(int)+sizeof(int)+sizeof(size_t)+(len*sizeof(char))) == -1) {
        return -1;
    }

    if (read(fcli, &r, sizeof(ssize_t)) == -1) {
        return -1;
    }

    free(request);
    printf("return write: %ld\n", r);
    return r;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    /*
    int r;
    ssize_t r1;
    ssize_t size;
    char *ptr;
    char buf[20];
    char request[20];

    r = snprintf(request, sizeof(request), "%c%d%d%ld", '6', session_id, fhandle, len);
    if (r < 0) {
        return -1;
    }

    r1 = write(fserv, request, sizeof(char)*(size_t)r);
    if (r1 <= 0) {
        return -1;
    }

    r1 = read(fcli, buf, sizeof(int));
    if (r1 <= 0) {
        return -1;
    }

    size = (ssize_t) strtol(buf, &ptr, 10);
    if (size < 0) {
        return -1;
    }

    r1 = read(fcli, buffer, sizeof(char)*(size_t)size);
    if (r1 <= 0) {
        return -1;
    }

    return (ssize_t) r1;
    */
    /* NEW IMPLEMENTATION */
    ssize_t r;
    char op_code = '6';
    char request[20];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+1, &session_id, sizeof(int));
    memcpy(request+1+sizeof(int), &fhandle, sizeof(int));
    memcpy(request+1+sizeof(int)+sizeof(int), &len, sizeof(size_t));

    if (write(fserv, request, 1+sizeof(int)+sizeof(int)+sizeof(size_t)) == -1) {
        return -1;
    }

    if (read(fcli, &r, sizeof(ssize_t)) == -1) {
        return -1;
    }

    printf("read: %ld\n", r);
    if (read(fcli, buffer, (sizeof(char)*(size_t)r)) == -1) {
        return -1;
    }
    
    return r;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    /*
    int r;
    ssize_t r1;
    char *ptr;
    char buf[20];
    char request[10];

    r = snprintf(request, sizeof(request), "%c%d", '7', session_id);
    if (r < 0) {
        return -1;
    }

    r1 = write(fserv, request, sizeof(char)*(size_t)r);
    if (r1 <= 0) {
        return -1;
    }

    r1 = read(fcli, buf, sizeof(int));
    if (r1 <= 0) {
        return -1;
    }

    r = (int) strtol(buf, &ptr, 10);
    return r;
    */

    /* NEW IMPLEMENTATION */
    int r;
    char op_code = '7';
    char request[10];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+1, &session_id, sizeof(int));

    if (write(fserv, request, 1+sizeof(int)) == -1) {
        return -1;
    }

    if (read(fcli, &r, sizeof(int)) == -1) {
        return -1;
    }

    return r;
}
