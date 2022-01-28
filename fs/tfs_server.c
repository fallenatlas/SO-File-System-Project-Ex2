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
// Mutex to protect above data.
static pthread_mutex_t lock;

void sendSucessToClient(int fclient) {
    int return_value = 0;
    if (write(fclient, &return_value, sizeof(int)) == -1) {
        printf("Couldn't write to client\n");
    }
}

void sendErrorToClient(int fclient) {
    int return_value = -1;
    if (write(fclient, &return_value, sizeof(int)) == -1) {
        printf("Couldn't write to client\n");
    }
}

void terminateClientSession(int session_id) {
    if (pthread_mutex_lock(&lock) != 0) {}
        //return -1;
    free_sessions[session_id] = FREE;
    number_active_sessions--;
    if (pthread_mutex_unlock(&lock) != 0) {}
        //return -1;
}

void processMount(int fclient, int session_id) {
    int s_id = session_id;
    if (sessions[session_id].count > 1) {
        sessions[session_id].count = 1;
    }
    if (write(fclient, &s_id, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
        /*
        if (close(fclient) == -1) {
            printf("[ERR]: close failed: %s\n", strerror(errno));
        }
        */
    }
}

void processUnmount(int fclient, int session_id) {
    terminateClientSession(session_id);
    sendSucessToClient(fclient);
    close(fclient);
}

void processOpen(int fclient, int session_id, char *name, int flags) {
    //TO DO
    int fhandle = tfs_open(name, flags);
    if (write(fclient, &fhandle, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }
}

void processClose(int fclient, int session_id, int fhandle) {
    //TO DO
    int r = tfs_close(fhandle);
    if (write(fclient, &r, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }
}

void processWrite(int fclient, int session_id, int fhandle, char *buffer, size_t to_write) {
    //TO DO
    ssize_t nbytes = tfs_write(fhandle, buffer, to_write);
    //printf("nbytes: %d\n", nbytes);
    if (write(fclient, &nbytes, sizeof(ssize_t)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }

    free(buffer);
}

void processRead(int fclient, int session_id, int fhandle, size_t len) {
    char *buffer = malloc(sizeof(char)*len);
    ssize_t nbytes = tfs_read(fhandle, buffer, len);
    //buffer[nbytes] = '\0';
    //printf("buffer in server: %d, %s\n", nbytes, buffer);
    char *response = (char*) malloc(sizeof(int)+((size_t)nbytes*sizeof(char)));
    //TO DO: send nbytes and buffer
    memcpy(response, &nbytes, sizeof(ssize_t));
    strncpy(response+sizeof(ssize_t), buffer, sizeof(char)*(size_t)nbytes);
    if (write(fclient, response, sizeof(ssize_t)+(sizeof(char)*(size_t)nbytes)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }

    free(buffer);
    free(response);
    printf("read sucess\n");
}

void processShutdown(int fclient, int session_id) {
    //TO DO
    int r = tfs_destroy_after_all_closed();
    if (write(fclient, &r, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }

    // don't forget to actually shutdown the server plsss
}

void threadProcessRequest(int fclient, r_args *request, int session_id) {
    int op_code = request->op_code;
    switch(op_code) {
        case TFS_OP_CODE_MOUNT :
            processMount(fclient, session_id);
            break;
        case TFS_OP_CODE_UNMOUNT :
            processUnmount(fclient, session_id);
            break;
        case TFS_OP_CODE_OPEN :
            processOpen(fclient, session_id, request->file_name, request->flags);
            break;
        case TFS_OP_CODE_CLOSE :
            processClose(fclient, session_id, request->fhandle);
            break;
        case TFS_OP_CODE_WRITE :
            processWrite(fclient, session_id, request->fhandle, request->buffer, request->size);
            break;
        case TFS_OP_CODE_READ :
            processRead(fclient, session_id, request->fhandle, request->size);
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED :
            processShutdown(fclient, session_id);
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
        threadProcessRequest(sessions[id].fcli, sessions[id].requests[consptr], id);
        //free(session[id].requests[consptr]);
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
    printf("acquired lock\n");
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

    if (pthread_mutex_init(&lock, 0) != 0)
        exit(1);

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
    if (pthread_mutex_lock(&lock) != 0)
        return -1;
    if (number_active_sessions < S) {
        for (int i = 0; i < S; i++) {
            if (free_sessions[i] == FREE) {
                number_active_sessions++;
                if (pthread_mutex_unlock(&lock) != 0)
                    return -1;
                return i;
            }
        }
    }
    if(pthread_mutex_unlock(&lock) != 0)
        return -1;
    return -1;
}


int processRequest(char *buf, int fserv) {
    int op_code;
    char *ptr;
    op_code = (int) strtol(buf, &ptr, 10);

    int session_id;
    r_args *request = (r_args*) malloc(sizeof(r_args));
    request->op_code = op_code;

    switch(op_code) {
        case TFS_OP_CODE_MOUNT :;
            printf("op: %d, mount\n", op_code);
            int fclient;
            char client_pipe[40];
            if (read(fserv, client_pipe, sizeof(char)*40) == -1) {
                return -1;
            }
            if ((fclient = open(client_pipe, O_WRONLY)) == -1) {
                return -1;
            }
            session_id = findSessionId();
            if (session_id == -1) {
                sendErrorToClient(fclient);
                close(fclient);
            }
            sessions[session_id].fcli = fclient;
            sendRequestToThread(session_id, request);
            printf("finished mount, %d\n", sessions[session_id].fcli);
            break;
        case TFS_OP_CODE_UNMOUNT :;
            printf("op: %d, unmount\n", op_code);
            if (read(fserv, &session_id, sizeof(int)) == -1) {
                //sendErrorToClient(sessions[session_id].fcli);
            }
            printf("session_id: %d\n", session_id);
            sendRequestToThread(session_id, request);
            printf("finished unmount\n");
            break;
        case TFS_OP_CODE_OPEN :;
            printf("op: %d, open\n", op_code);
            if (read(fserv, &session_id, sizeof(int)) == -1) {
                //sendErrorToClient(sessions[session_id].fcli);
            }
            printf("session_id: %d\n", session_id);
            if (read(fserv, request->file_name, 40*sizeof(char)) == -1) {
                printf("send error\n");
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("file name: %s\n", request->file_name);
            if (read(fserv, &request->flags, sizeof(int)) == -1) {
                printf("send error\n");
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("flags: %d\n", request->flags);
            sendRequestToThread(session_id, request);
            printf("finished open\n");
            break;
        case TFS_OP_CODE_CLOSE :;
            printf("op: %d, close\n", op_code);
            if (read(fserv, &session_id, sizeof(int)) == -1) {
                //sendErrorToClient(sessions[session_id].fcli);
            }
            printf("session_id: %d\n", session_id);
            if (read(fserv, &request->fhandle, sizeof(int)) == -1) {
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("fhandle: %d\n", request->fhandle);
            sendRequestToThread(session_id, request);
            printf("finished close\n");
            break;
        case TFS_OP_CODE_WRITE :;
            printf("op: %d, write\n", op_code);
            if (read(fserv, &session_id, sizeof(int)) == -1) {
                //sendErrorToClient(sessions[session_id].fcli);
            }
            printf("session_id: %d\n", session_id);
            if (read(fserv, &request->fhandle, sizeof(int)) == -1) {
                printf("send error\n");
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("fhandle: %d\n", request->fhandle);
            if (read(fserv, &request->size, sizeof(size_t)) == -1) {
                printf("send error\n");
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("len: %ld\n", request->size);
            request->buffer = (char*) malloc(sizeof(char)*(request->size));
            if (read(fserv, request->buffer, sizeof(char)*(request->size)) == -1) {
                printf("send error\n");
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("buffer: %s\n", request->buffer);
            sendRequestToThread(session_id, request);
            printf("finished write\n");
            break;
        case TFS_OP_CODE_READ :
            printf("op: %d, read\n", op_code);
            if (read(fserv, &session_id, sizeof(int)) == -1) {
                //sendErrorToClient(sessions[session_id].fcli);
            }
            printf("session_id: %d\n", session_id);
            if (read(fserv, &request->fhandle, sizeof(int)) == -1) {
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("fhandle: %d\n", request->fhandle);
            if (read(fserv, &request->size, sizeof(size_t)) == -1) {
                sendErrorToClient(sessions[session_id].fcli);
            }
            printf("len: %ld\n", request->size);
            sendRequestToThread(session_id, request);
            printf("finished read\n");
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED :
            printf("op: %d, shutdown\n", op_code);
            if (read(fserv, &session_id, sizeof(int)) == -1) {
                //sendErrorToClient(sessions[session_id].fcli);
            }
            printf("session_id: %d\n", session_id);
            sendRequestToThread(session_id, request);
            printf("finished shutdown\n");
            break;
        default:
            return -1;
    }

    printf("returning from process request\n");
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
        if (read(fserv, buf, sizeof(char)) == 0) {
            close(fserv);
            fserv = open(pipename, O_RDONLY);
            continue;
        }
        //r = read(fserv, buf, sizeof(char)*40);
        //printf("after read: %s, %ld\n", buf, r);
        processRequest(buf, fserv);
    }

    close(fserv);
    unlink(pipename);
    return 0;
}