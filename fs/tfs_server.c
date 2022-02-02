#include "tfs_server.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

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
static int fserv;

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

void shutdownServer() {
    //TO DO
    //close(fserv);
    exit(EXIT_FAILURE);
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
    int fhandle = tfs_open(name, flags);
    if (write(fclient, &fhandle, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }
}

void processClose(int fclient, int session_id, int fhandle) {
    int r = tfs_close(fhandle);
    if (write(fclient, &r, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }
}

void processWrite(int fclient, int session_id, int fhandle, char *buffer, size_t to_write) {
    int nbytes = (int) tfs_write(fhandle, buffer, to_write);
    //printf("nbytes: %d\n", nbytes);
    if (write(fclient, &nbytes, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }
    free(buffer);
}

void processRead(int fclient, int session_id, int fhandle, size_t len) {
    char *buffer = malloc(sizeof(char)*len);
    if (buffer == NULL)
        exit(EXIT_FAILURE);
    int nbytes = (int) tfs_read(fhandle, buffer, len);
    //buffer[nbytes] = '\0';
    //printf("buffer in server: %d, %s\n", nbytes, buffer);
    char *response = (char*) malloc(sizeof(int)+((size_t)nbytes*sizeof(char)));
    if (response == NULL)
        exit(EXIT_FAILURE);

    memcpy(response, &nbytes, sizeof(int));
    strncpy(response+sizeof(int), buffer, sizeof(char)*(size_t)nbytes);
    if (write(fclient, response, sizeof(int)+(sizeof(char)*(size_t)nbytes)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }

    free(buffer);
    free(response);
    printf("read sucess\n");
}

void processShutdown(int fclient, int session_id) {
    int r = tfs_destroy_after_all_closed();
    if (write(fclient, &r, sizeof(int)) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        close(fclient);
    }
    close(fserv);
    exit(EXIT_SUCCESS);
    // don't forget to actually shutdown the server plsss
}

r_args* get_request(int session_id) {
    return &(sessions[session_id].request);
}

void threadProcessRequest(int session_id) {
    //int op_code = request->op_code;
    r_args *request = get_request(session_id);
    int fclient = sessions[session_id].fcli;
    switch(request->op_code) {
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
            return;
    }
}

void *working_thread(void *arg){
    int id = *((int *) arg);
    free(arg);
    //int consptr = 0;
    while (1) {
        if (pthread_mutex_lock(&sessions[id].prod_cons_mutex) != 0)
            shutdownServer();
        while (sessions[id].count == 0)
            if (pthread_cond_wait(&sessions[id].cons, &sessions[id].prod_cons_mutex) != 0)
                shutdownServer();
        sessions[id].count--;
        threadProcessRequest(id);
        /*threadProcessRequest(sessions[id].fcli, sessions[id].requests[consptr], id);
        free(session[id].requests[consptr]);
        consptr++;
        if (consptr == N)
            consptr = 0;
        sessions[id].count--;
        if (pthread_cond_signal(&sessions[id].prod) != 0)
            shutdownServer();*/
        if (pthread_mutex_unlock(&sessions[id].prod_cons_mutex) != 0)
            shutdownServer();
    }
}

void sendRequestToThread(int id) {
    if (pthread_mutex_lock(&sessions[id].prod_cons_mutex) != 0)
        shutdownServer();
    printf("acquired lock\n");
    /*while (sessions[id].count == N) {
        if (pthread_cond_wait(&sessions[id].prod, &sessions[id].prod_cons_mutex) != 0)
            shutdown_server();
    }
    
    sessions[id].requests[sessions[id].prodptr] = request;
    sessions[id].prodptr++;
    if (sessions[id].prodptr == N)
        sessions[id].prodptr = 0;
    sessions[id].request = request;*/

    // Always empty when sending request
    sessions[id].count++;
    if (pthread_cond_signal(&sessions[id].cons) != 0)
        shutdownServer();
    if (pthread_mutex_unlock(&sessions[id].prod_cons_mutex) != 0)
        shutdownServer();
}

void prepareServer() {
    /* Initialize tfs */
    if (tfs_init() == -1) {
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < S; i++) {
        free_sessions[i] = FREE;
    }
    number_active_sessions = 0;

    if (pthread_mutex_init(&lock, 0) != 0)
        exit(EXIT_FAILURE);

    for (int s = 0; s < S; s++) {
        //sessions[s].prodptr = 0;
        sessions[s].count = 0;
        if (pthread_mutex_init(&sessions[s].prod_cons_mutex, NULL) != 0)
            exit(EXIT_FAILURE);
        if (pthread_cond_init(&sessions[s].cons, NULL) != 0)
            exit(EXIT_FAILURE);
        /*if (pthread_cond_init(&sessions[s].prod, NULL) != 0)
            exit(EXIT_FAILURE);*/
    }

    /* Create S working threads */
    pthread_t tid[S];

    for (int i = 0; i < S; i++) {
        int *j = (int *) malloc(sizeof(int));
        if (j == NULL)
            exit(EXIT_FAILURE);
        (*j) = i;
        if (pthread_create (&tid[i], NULL, working_thread, (void*) j) != 0)
            exit(EXIT_FAILURE);
    }
}

int findSessionId() {
    if (pthread_mutex_lock(&lock) != 0)
        shutdownServer();
    if (number_active_sessions < S) {
        for (int i = 0; i < S; i++) {
            if (free_sessions[i] == FREE) {
                free_sessions[i] = TAKEN;
                number_active_sessions++;
                if (pthread_mutex_unlock(&lock) != 0)
                    shutdownServer();
                return i;
            }
        }
    }
    if(pthread_mutex_unlock(&lock) != 0)
        shutdownServer();
    return -1;
}


int mountProducer() {
    int fclient;
    int session_id;
    char client_pipe[SIZE_CLIENT_PIPE_PATH];

    if (read(fserv, client_pipe, sizeof(char)*SIZE_CLIENT_PIPE_PATH) == -1) {
        return -1;
    }

    printf("client pipe: %s\n", client_pipe);
    if ((fclient = open(client_pipe, O_WRONLY)) == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // Try to associate a session to client
    session_id = findSessionId();
    if (session_id == -1) {
        sendErrorToClient(fclient);
        close(fclient);
        return -1;
    }
    sessions[session_id].fcli = fclient;
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_MOUNT;
    sendRequestToThread(session_id);
    printf("finished mount, %d\n", sessions[session_id].fcli);
    return 0;
}

void unmountProducer() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        //sendErrorToClient(sessions[session_id].fcli);
    }
    printf("session_id: %d\n", session_id);
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_UNMOUNT;
    sendRequestToThread(session_id);
    printf("finished unmount\n");
}
void openProducer() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        //sendErrorToClient(sessions[session_id].fcli);
    }
    printf("session_id: %d\n", session_id);

    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_OPEN;
    if (read(fserv, request->file_name, SIZE_FILE_NAME*sizeof(char)) == -1) {
        printf("send error\n");
        sendErrorToClient(sessions[session_id].fcli);
    }
    printf("file name: %s\n", request->file_name);
    if (read(fserv, &request->flags, sizeof(int)) == -1) {
        printf("send error\n");
        sendErrorToClient(sessions[session_id].fcli);
    }
    printf("flags: %d\n", request->flags);
    sendRequestToThread(session_id);
    printf("finished open\n");
}

void closeProducer() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        //sendErrorToClient(sessions[session_id].fcli);
    }
    printf("session_id: %d\n", session_id);

    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_CLOSE;
    if (read(fserv, &request->fhandle, sizeof(int)) == -1) {
        sendErrorToClient(sessions[session_id].fcli);
    }
    printf("fhandle: %d\n", request->fhandle);
    sendRequestToThread(session_id);
    printf("finished close\n");
}

void writeProducer() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        //sendErrorToClient(sessions[session_id].fcli);
    }
    printf("session_id: %d\n", session_id);
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_WRITE;
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
    if (request->buffer == NULL)
        exit(EXIT_FAILURE);
    if (read(fserv, request->buffer, sizeof(char)*(request->size)) == -1) {
        printf("send error\n");
        sendErrorToClient(sessions[session_id].fcli);
    }
    printf("buffer: %s\n", request->buffer);
    sendRequestToThread(session_id);
    printf("finished write\n");
}

void readProducer() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        //sendErrorToClient(sessions[session_id].fcli);
    }
    printf("session_id: %d\n", session_id);
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_READ;
    if (read(fserv, &request->fhandle, sizeof(int)) == -1) {
        sendErrorToClient(sessions[session_id].fcli);
    }
    printf("fhandle: %d\n", request->fhandle);
    if (read(fserv, &request->size, sizeof(size_t)) == -1) {
        sendErrorToClient(sessions[session_id].fcli);
    }
    printf("len: %ld\n", request->size);
    sendRequestToThread(session_id);
    printf("finished read\n");
}

void shutdownProducer() {
    int session_id;
    if (read(fserv, &session_id, sizeof(int)) == -1) {
        //sendErrorToClient(sessions[session_id].fcli);
    }
    printf("session_id: %d\n", session_id);
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    sendRequestToThread(session_id);
    printf("finished shutdown\n");

}

void processRequest(char *char_opcode) {
    int op_code;
    char *ptr;
    op_code = (int) strtol(char_opcode, &ptr, 10);

    /* Allocates memory for new request
    r_args *request = (r_args*) malloc(sizeof(r_args));
    if (request == NULL) {
        shutdown_server();
        exit(EXIT_FAILURE);
    }
    request->op_code = op_code;*/
    // Determine type of request and calls function to produce
    // request for thread
    switch(op_code) {
        case TFS_OP_CODE_MOUNT :
            printf("op: %d, mount\n", op_code);
            mountProducer();
            break;
        case TFS_OP_CODE_UNMOUNT :
            printf("op: %d, unmount\n", op_code);
            unmountProducer();
            break;
        case TFS_OP_CODE_OPEN :
            printf("op: %d, open\n", op_code);
            openProducer();
            break;
        case TFS_OP_CODE_CLOSE :
            printf("op: %d, close\n", op_code);
            closeProducer();
            break;
        case TFS_OP_CODE_WRITE :
            printf("op: %d, write\n", op_code);
            writeProducer();
            break;
        case TFS_OP_CODE_READ :
            printf("op: %d, read\n", op_code);
            readProducer();
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED :
            printf("op: %d, shutdown\n", op_code);
            shutdownProducer();
            break;
        default:
            return;
    }

    printf("returning from process request\n");
    return;
}


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    //int fserv;
    char opcode[1];
    signal(SIGPIPE, SIG_IGN);

    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    prepareServer();

    // Create and open pipe server
    if (mkfifo(pipename, 0640) < 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do {
        fserv = open(pipename, O_RDONLY);
    } while(fserv == -1 && errno == EINTR);
        
    if (fserv == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        //shutdownServer();
        exit(EXIT_FAILURE);
    }
    // Server reads requests from clients
    ssize_t r;
    while (1) {
        do {
            r = read(fserv, opcode, sizeof(char));
        } while (r == -1 && errno == EINTR);
        
        if (r == 0) {
            // All sessions closed open again pipe server
            close(fserv);
            do {
                fserv = open(pipename, O_RDONLY);
            } while(fserv == -1 && errno == EINTR);
        
            if (fserv == -1){
                fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            continue;
        }
        else if (r == -1) {
            // Couldn't read opcode
            shutdownServer();
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        processRequest(opcode);
    }

    //close(fserv);
    //unlink(pipename);
    return 0;
}