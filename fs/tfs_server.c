#include "tfs_server.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>


static int number_active_sessions;
static char free_sessions[S];
static session_info sessions[S];

void processMount(char *client_pipe, int session_id) {
    //TO DO: open client pipe, send message
    if (write(sessions[session_id].fcli, "0", sizeof(int)) <= 0) {
        printf("Couldn't write to client\n");
    }
}

void processUnmount(int session_id) {
    //TO DO: close client pipe, change number_active_sessions, free session
}

void processOpen(char *client_pipe, char *name, int flags) {
    //TO DO
}

void processClose(char *client_pipe, int fhandle) {
    //TO DO
}

void processWrite(char *client_pipe, int fhandle, char *buffer, size_t size) {
    //TO DO
}

void processRead(int fhandle, size_t size) {
    //TO DO
}

void processShutdown(char *client_pipe) {
    //TO DO
}

void threadProcessRequest(char *client_pipe, r_args *request, int session_id) {
    int op_code = request->op_code;
    switch(op_code) {
        case TFS_OP_CODE_MOUNT :
            processMount(client_pipe, session_id);
            break;
        case TFS_OP_CODE_UNMOUNT :
            processUnmount(session_id);
            break;
        case TFS_OP_CODE_OPEN :
            processOpen(client_pipe, request->file_name, request->flags);
            break;
        case TFS_OP_CODE_CLOSE :
            processClose(client_pipe, request->fhandle);
            break;
        case TFS_OP_CODE_WRITE :
            processWrite(client_pipe, request->fhandle, request->buffer, request->size);
            break;
        case TFS_OP_CODE_READ :
            processRead(request->fhandle, request->size);
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED :
            processShutdown(client_pipe);
            break;
        default:
            exit(1);
    }

}

void *working_thread(void *arg){
    int id = *((int *) arg);
    free(arg);
    int consptr = 0;
    while (1) {
        pthread_mutex_lock(&sessions[id].prod_cons_mutex);
        while (sessions[id].count == 0)
            pthread_cond_wait(&sessions[id].cons, &sessions[id].prod_cons_mutex);
        threadProcessRequest(sessions[id].client_pipe_path, sessions[id].requests[consptr], id);
        consptr++;
        if (consptr == N)
            consptr = 0;
        sessions[id].count--;
        pthread_cond_signal(&sessions[id].prod);
        pthread_mutex_unlock(&sessions[id].prod_cons_mutex);
    }
}

void sendRequestToThread(int id, r_args *request) {
    pthread_mutex_lock(&sessions[id].prod_cons_mutex);
    printf("aquired lock\n");
    while (sessions[id].count == N) {
        pthread_cond_wait(&sessions[id].prod, &sessions[id].prod_cons_mutex);
    }
    sessions[id].requests[sessions[id].prodptr] = request;
    sessions[id].prodptr++;
    if (sessions[id].prodptr == N)
        sessions[id].prodptr = 0;
    sessions[id].count++;
    pthread_cond_signal(&sessions[id].cons);
    pthread_mutex_unlock(&sessions[id].prod_cons_mutex);
}

void prepareServer() {
    /* Initialize tfs */
    if (tfs_init() == -1) {
        exit(1);
    }

    for (size_t i = 0; i < S; i++) {
        free_sessions[i] = FREE;
    }
    number_active_sessions = 0;

    for (int s = 0; s < S; s++) {
        sessions[s].prodptr = 0;
        sessions[s].count = 0;
        if (pthread_mutex_init(&sessions[s].prod_cons_mutex, NULL) != 0)
            exit(1);
        if (pthread_cond_init(&sessions[s].cons, NULL) != 0)
            exit(1);
        if (pthread_cond_init(&sessions[s].prod, NULL) != 0)
            exit(1);
    }

    /* Create S working threads */
    pthread_t tid[S];

    for (int i = 0; i < S; i++) {
        int *j = (int *) malloc(sizeof(int));
        (*j) = i;
        if (pthread_create (&tid[i], NULL, working_thread, (void*) j) != 0)
            exit(1);
    }
}

/* Add locks for free sessions */
int findSessionId() {
    if (number_active_sessions < S) {
        for (int i = 0; i < S; i++) {
            if (free_sessions[i] == FREE) {
                number_active_sessions++;
            return i;
            }
        }
    }
    return -1;
}


int processRequest(char *buf, int fserv) {
    int op_code;
    char *ptr;
    op_code = (int) strtol(buf, &ptr, 10);
    printf("op: %d\n", op_code);

    r_args *request = (r_args*) malloc(sizeof(r_args));
    request->op_code = op_code;

    switch(op_code) {
        case TFS_OP_CODE_MOUNT :
            printf("here\n");
            int fclient;
            int session_id;
            char client_pipe[40];
            if (read(fserv, client_pipe, sizeof(char)*40) <= 0) {
                return -1;
            }
            if ((fclient = open(client_pipe, O_WRONLY)) == -1) {
                return -1;
            }
            session_id = findSessionId();
            if (session_id == -1) {
                if (write(fclient, "-1", sizeof(int)) <= 0) {
                    return -1;
                }
                close(fclient);
            }
            sessions[session_id].fcli = fclient;
            printf("before sending request\n");
            sendRequestToThread(session_id, request);
            printf("finished\n");
            break;
        case TFS_OP_CODE_UNMOUNT :
            break;
        case TFS_OP_CODE_OPEN :
            break;
        case TFS_OP_CODE_CLOSE :
            break;
        case TFS_OP_CODE_WRITE :
            break;
        case TFS_OP_CODE_READ :
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED :
            break;
        default:
            exit(1);
    }

    printf("returning\n");
    return 0;
}


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    ssize_t r;
    int fserv;
    char buf[4096];
    memset(buf, '\0', 4096);
    printf("before unlink\n");

    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    printf("after unlink\n");

    
    prepareServer();
    printf("after prepareServer\n");
    if (mkfifo(pipename, 0640) < 0) {
        exit(1);
    }
    printf("after mkfifo\n");
    if ((fserv = open(pipename, O_RDONLY)) == -1) {
        exit(1);
    }
    printf("opened pipe\n");
    //TO DO: implement producer in server

    while (1) {
        r = read(fserv, buf, sizeof(char));
        if (r <= 0) {
            break;
        }
        printf("after read: %s, %ld\n", buf, r);
        //r = read(fserv, buf, sizeof(char)*40);
        //printf("after read: %s, %ld\n", buf, r);
        processRequest(buf, fserv);
        printf("returned\n");
    }

    close(fserv);
    unlink(pipename);
    return 0;
}