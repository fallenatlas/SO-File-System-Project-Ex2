#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "operations.h"

#define SIZE_CLIENT_PIPE_PATH 40
#define SIZE_FILE_NAME 40
#define S 10
#define N 15

typedef struct {
    int op_code;
    int fhandle;
    int flags;
    size_t size;
    char *buffer;
    char file_name[SIZE_FILE_NAME];
} r_args;

typedef struct {
    int fcli;
    r_args *requests[N];
    int prodptr;
    int count;
    pthread_mutex_t prod_cons_mutex;
    pthread_cond_t cons;
    pthread_cond_t prod;
} session_info;


#endif // TFS_SERVER_H