#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h> // -> unlink
#include <sys/stat.h> // -> mkfifo
#include <fcntl.h> // -> 0_RDONLY
#include <errno.h>
#include <signal.h>

int session_id; // Client session id
int fserv; // Server pipe
int fcli; // Client pipe
char const *client_pipe_name;

/*
 * Writes a message in server pipe and retry if possible
 * Returns write return value -1 if an error ocurred
 * during writing.
 */
ssize_t send_msg(char const *request, size_t len) {
    ssize_t ret;
    do {
       ret = write(fserv, request, len); 
    } while (ret < 0 && errno == EINTR);
    return ret;
}

/*
 * Reads an integer in server pipe and retry if possible
 * Returns read return value -1 if an error ocurred
 * during reading.
 */
ssize_t receive_msg(int *r) {
    ssize_t read_check;
    do {
        read_check = read(fcli, r, sizeof(int));
        if (read_check == 0)
            // Server closed write end of client pipe
            exit(EXIT_FAILURE);
    } while (read_check == -1 && errno == EINTR);
    return read_check;
}

/*
 * Reads a string in server pipe and retry if possible
 * Returns read return value -1 if an error ocurred
 * during reading.
 */
ssize_t receive_buffer(char *buffer, size_t r) {
    ssize_t read_check;
    do {
        read_check = read(fcli, buffer, (sizeof(char)*r));
        if (read_check == 0)
            // Server closed client pipe
            exit(EXIT_FAILURE);
    } while (read_check == -1 && errno == EINTR);
    return read_check;
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    int s_id;
    char op_code = '1';
    char request[MAX_MOUNT_REQUEST];
    memset(request, '\0', sizeof(request));

    // Create client pipe.
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_pipe_path,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (mkfifo(client_pipe_path, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo(%s) failed: %s\n", client_pipe_path,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    client_pipe_name = client_pipe_path;

    printf("going to open server pipe\n");
    // Open client pipe
    do {
        fserv = open(server_pipe_path, O_WRONLY);
    } while(fserv == -1 && errno == EINTR);
    printf("after opening server pipe\n");
        
    if (fserv == -1){
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Write request to server pipe
    request[0] = op_code;
    strncpy(&request[1], client_pipe_path, SIZE_CLIENT_PIPE_PATH*sizeof(char));
    if (send_msg(request, sizeof(char)*(SIZE_CLIENT_PIPE_PATH+1)) < 0)
        // Write failed
        return -1;

    // Open server pipe
    do {
        fcli = open(client_pipe_path, O_RDONLY);
    } while(fcli == -1 && errno == EINTR);
        
    if (fcli == -1) {
        close(fserv);
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    printf("after opening client pipe\n");
    // Read response from the client pipe.
    if (receive_msg(&s_id) == -1)
        // Read failed
        return -1;

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
    int r;
    char op_code = '2';
    char request[MAX_UNMOUNT_REQUEST];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+sizeof(char), &session_id, sizeof(int));

    if (send_msg(request, SIZE_UNMOUNT) < 0)
        // Write failed
        return -1;

    if (receive_msg(&r) == -1)
        // Read failed
        return -1;

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
    int fhandle;
    char op_code = '3';
    char request[MAX_OPEN_REQUEST];
    memset(request, '\0', sizeof(request));
    
    memcpy(request, &op_code, sizeof(char));
    memcpy(request+sizeof(char), &session_id, sizeof(int));
    strncpy(request+sizeof(char)+sizeof(int), name, SIZE_FILE_NAME_PATH*sizeof(char));
    memcpy(request+sizeof(char)+sizeof(int)+(SIZE_FILE_NAME_PATH*sizeof(char)), &flags, sizeof(int));

    if(send_msg(request, sizeof(char)+sizeof(int)+(SIZE_FILE_NAME_PATH*sizeof(char))+sizeof(int)) < 0)
        // Write failed
        return -1;

    if (receive_msg(&fhandle) == -1)
        // Read failed
        return -1;

    printf("fhandle: %d\n", fhandle);
    return fhandle;
}

int tfs_close(int fhandle) {
    int r;
    char op_code = '4';
    char request[MAX_CLOSE_REQUEST];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+sizeof(char), &session_id, sizeof(int));
    memcpy(request+sizeof(char)+sizeof(int), &fhandle, sizeof(int));

    if (send_msg(request, SIZE_CLOSE) < 0)
        // Write failed
        return -1;

    if (receive_msg(&r) == -1)
        // Read failed
        return -1;

    printf("return close: %d\n", r);
    return r;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    int r;
    char op_code = '5';
    char *request = (char*) malloc(SIZE_WRITE+sizeof(size_t)+len*sizeof(char));
    if (request == NULL)
        return -1;
    request[0] = op_code;
    memcpy(request+sizeof(char), &session_id, sizeof(int));
    memcpy(request+sizeof(char)+sizeof(int), &fhandle, sizeof(int));
    memcpy(request+sizeof(char)+sizeof(int)+sizeof(int), &len, sizeof(size_t));
    strncpy(request+sizeof(char)+sizeof(int)+sizeof(int)+sizeof(size_t), buffer, len*sizeof(char));

    if (send_msg(request, SIZE_WRITE+sizeof(size_t)+(len*sizeof(char))) < 0) {
        // Write failed
        free(request);
        return -1;
    }

    if (receive_msg(&r) == -1) {
        // Read failed
        free(request);
        return -1;
    }

    free(request);
    printf("return write: %d\n", r);
    return (ssize_t) r;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    int r;
    char op_code = '6';
    char request[MAX_READ_REQUEST];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+sizeof(char), &session_id, sizeof(int));
    memcpy(request+sizeof(char)+sizeof(int), &fhandle, sizeof(int));
    memcpy(request+sizeof(char)+sizeof(int)+sizeof(int), &len, sizeof(size_t));

    if (send_msg(request, SIZE_READ) < 0)
        // Write failed
        return -1;

    if (receive_msg(&r) == -1)
        // Read failed
        return -1;

    printf("read: %d\n", r);
    // Read buffer that was requested to read
    if (receive_buffer(buffer, (size_t)r) == -1)
        // Read failed
        return -1;
    

    return (ssize_t) r;
}

int tfs_shutdown_after_all_closed() {
    int r;
    char op_code = '7';
    char request[MAX_SHUTDOWN_REQUEST];
    memset(request, '\0', sizeof(request));

    request[0] = op_code;
    memcpy(request+sizeof(char), &session_id, sizeof(int));

    if (send_msg(request, SIZE_SHUTDOWN) < 0)
        // Write failed
        return -1;

    if (receive_msg(&r) == -1)
        // Read failed
        return -1;

    return r;
}