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
static char free_sessions[S];  // Indicates which sessions are free
static session_info sessions[S];
static pthread_mutex_t lock; // Mutex to protect above data.
static int fserv; // Server pipe

void assignHandler() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        exit(EXIT_FAILURE);
    }
}

/* Reads an integer from server pipe and retry if possible */
ssize_t readInt(int *ri) {
    ssize_t read_check;
    do {
        read_check = read(fserv, ri, sizeof(int));
    } while (read_check == -1 && errno == EINTR);
    return read_check;
}

/* Reads a size_t from server pipe and retry if possible */
ssize_t readSizeT(size_t *rs) {
    ssize_t read_check;
    do {
        read_check = read(fserv, rs, sizeof(size_t));
    } while (read_check == -1 && errno == EINTR);
    return read_check;
}

/* Writes an integer in client pipe and retry if possible */
ssize_t writeInt(int fclient, int wi) {
    ssize_t write_check;
    do {
        write_check = write(fclient, &wi, sizeof(int));
    } while (write_check == -1 && errno == EINTR);
    return write_check;
}

/* Reads a string of size r
 * from server pipe and retry if possible */
ssize_t readBuffer(char *buffer, size_t r) {
    ssize_t read_check;
    do {
        read_check = read(fserv, buffer, (sizeof(char)*r));
    } while (read_check == -1 && errno == EINTR);
    return read_check;
}

/* In case of error shutdown the server */
void shutdownServer() {
    close(fserv);
    exit(EXIT_FAILURE);
}

/* Free's a session */
void terminateClientSession(int session_id) {
    if (pthread_mutex_lock(&lock) != 0) {
        shutdownServer();
    }
    free_sessions[session_id] = FREE;
    number_active_sessions--;
    if (pthread_mutex_unlock(&lock) != 0) {
        shutdownServer();
    }
}

/* Close client pipe and prints a error message
 * if some error ocurred
 */
void tryClose(int fclient) {
    if (close(fclient) == -1) {
        printf("[ERR] Close failed: %s\n", strerror(errno));
    }
}

/* Tries to write an integer in client pipe if it
isn't possible to comunicate with client it terminates
its session.
 */
void tryWriteInt(int fclient, int session_id, int wi) {
    if (writeInt(fclient, wi) == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        tryClose(fclient);
    }
}

/* Sends a success message to client */
void sendSucessToClient(int fclient) {
    int return_value = 0;
    writeInt(fclient, return_value);
}

/* Sends an error message to client */
void sendErrorToClient(int fclient) {
    int return_value = -1;
    writeInt(fclient, return_value);
}

/* Sends an error message to client and terminates session
 * if failed */
void trySendErrorToClient(int fclient, int session_id) {
    int return_value = -1;
    tryWriteInt(fclient, session_id, return_value);
}

/* Sends session id to new client from client pipe */
void processMount(int fclient, int session_id) {
    tryWriteInt(fclient, session_id, session_id);
}

/* Unmounts client and sends message to client */
void processUnmount(int fclient, int session_id) {
    terminateClientSession(session_id);
    sendSucessToClient(fclient);
    tryClose(fclient);
}

/* Processes open requests and comunicates the result with client */
void processOpen(int fclient, int session_id, char *name, int flags) {
    int fhandle = tfs_open(name, flags);
    tryWriteInt(fclient, session_id, fhandle);
}

/* Processes close requests and comunicates the result with client */
void processClose(int fclient, int session_id, int fhandle) {
    int r = tfs_close(fhandle);
    tryWriteInt(fclient, session_id, r);
}

/* Processes write requests and comunicates the result with client */
void processWrite(int fclient, int session_id, int fhandle, char *buffer, size_t to_write) {
    int nbytes = (int) tfs_write(fhandle, buffer, to_write);
    tryWriteInt(fclient, session_id, nbytes);
    free(buffer); // Allocated in writeProducer
}

/* Processes read requests and comunicates the result with client */
void processRead(int fclient, int session_id, int fhandle, size_t len) {
    ssize_t write_check;
    char *buffer = malloc(sizeof(char)*len);
    if (buffer == NULL)
        shutdownServer();
    int nbytes = (int) tfs_read(fhandle, buffer, len);
    /* Create message to be delivered to client */
    char *response = (char*) malloc(sizeof(int)+((size_t)nbytes*sizeof(char)));
    if (response == NULL)
        shutdownServer();

    memcpy(response, &nbytes, sizeof(int));
    strncpy(response+sizeof(int), buffer, sizeof(char)*(size_t)nbytes);
    /* Try to send message to client and terminate client session if it 
    isn't possible to send it */
    do {
        write_check = write(fclient, response, sizeof(int)+(sizeof(char)*(size_t)nbytes));
    } while (write_check == -1 && errno == EINTR);
    if (write_check == -1) {
        printf("Unreachable Client: Terminating Session %d\n", session_id);
        terminateClientSession(session_id);
        tryClose(fclient);
    }

    free(buffer);
    free(response);
}

/* Processes shutdown requests, comunicates the result with client
 * and terminates server */
void processShutdown(int fclient, int session_id) {
    int r = tfs_destroy_after_all_closed();
    tryWriteInt(fclient, session_id, r);
    tryClose(fserv);
    exit(EXIT_SUCCESS);
}

/* Returns request to fill from session id*/
r_args* get_request(int session_id) {
    return &(sessions[session_id].request);
}

/* Determine the type of request and calls specific
 * function to process it */
void threadProcessRequest(int session_id) {
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

/* Each thread waits for new request and processes it */
void *working_thread(void *arg){
    int id = *((int *) arg);
    free(arg);
    assignHandler();
    while (1) {
        if (pthread_mutex_lock(&sessions[id].prod_cons_mutex) != 0)
            shutdownServer();
        while (sessions[id].count == 0)
            if (pthread_cond_wait(&sessions[id].cons, &sessions[id].prod_cons_mutex) != 0)
                shutdownServer();
        sessions[id].count--;
        threadProcessRequest(id);
        if (pthread_mutex_unlock(&sessions[id].prod_cons_mutex) != 0)
            shutdownServer();
    }
}

/* After request is ready signals thread, that is associated with the
 * session id passed as argument, that there is a new request to process*/
void sendRequestToThread(int id) {
    if (pthread_mutex_lock(&sessions[id].prod_cons_mutex) != 0)
        shutdownServer();
    // Always empty when sending request
    sessions[id].count++;
    if (pthread_cond_signal(&sessions[id].cons) != 0)
        shutdownServer();
    if (pthread_mutex_unlock(&sessions[id].prod_cons_mutex) != 0)
        shutdownServer();
}

/* Initializes everything that server needs 
 * before accepting requests from clients:
 * global variables, locks and threads */
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
        sessions[s].count = 0;
        if (pthread_mutex_init(&sessions[s].prod_cons_mutex, NULL) != 0)
            exit(EXIT_FAILURE);
        if (pthread_cond_init(&sessions[s].cons, NULL) != 0)
            exit(EXIT_FAILURE);
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

/* Tries to find free session. Returns
 * id of free session or -1 if there isn't one */
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

/*
 * Reads mount requests and sends it to respective
 * thread to process it
 */
void mountProducer() {
    int fclient;
    int session_id;
    char client_pipe[SIZE_CLIENT_PIPE_PATH];
    ssize_t ret;

    if ((ret = readBuffer(client_pipe, SIZE_CLIENT_PIPE_PATH)) < 0) {
        shutdownServer();
    }
    else if (ret == 0)
        return;

    // Open client pipe
    do {
        fclient = open(client_pipe, O_WRONLY);
    } while(fserv == -1 && errno == EINTR);
        
    if (fclient == -1){
        return;
    }

    // Try to associate a session to client
    session_id = findSessionId();
    if (session_id == -1) {
        sendErrorToClient(fclient);
        tryClose(fclient);
        return;
    }
    sessions[session_id].fcli = fclient;
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_MOUNT;
    sendRequestToThread(session_id);
    return;
}

/*
 * Reads unmount requests and sends it to respective
 * thread to process it
 */
void unmountProducer() {
    int session_id;
    ssize_t ret;
    if ((ret = readInt(&session_id)) < 0) {
        shutdownServer();
    }
    else if (ret == 0) {
        // There aren't write ends open
        return;
    }
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_UNMOUNT;
    sendRequestToThread(session_id);
}

/*
 * Reads open requests and sends it to respective
 * thread to process it
 */
void openProducer() {
    int session_id;
    ssize_t ret;

    if ((ret = readInt(&session_id)) < 0) {
        shutdownServer();
    }
    else if (ret == 0) {
        // There aren't write ends open
        return;
    }

    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_OPEN;
    if ((ret = readBuffer(request->file_name, SIZE_FILE_NAME)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    if ((ret =readInt(&request->flags)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    sendRequestToThread(session_id);
}

/*
 * Reads close requests and sends it to respective
 * thread to process it
 */
void closeProducer() {
    int session_id;
    ssize_t ret;

    if ((ret = readInt(&session_id)) < 0) {
        shutdownServer();
    }
    else if (ret == 0) {
        // There aren't write ends open
        return;
    }

    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_CLOSE;
    if ((ret = readInt(&request->fhandle)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    sendRequestToThread(session_id);
}

/*
 * Reads write requests and sends it to respective
 * thread to process it
 */
void writeProducer() {
    int session_id;
    ssize_t ret;

    if ((ret = readInt(&session_id)) < 0) {
        shutdownServer();
    }
    else if (ret == 0) {
        // There aren't write ends open
        return;
    }
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_WRITE;
    if ((ret = readInt(&request->fhandle)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    if ((ret = readSizeT(&request->size)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    request->buffer = (char*) malloc(sizeof(char)*(request->size));
    if (request->buffer == NULL)
        shutdownServer();

    if ((ret = readBuffer(request->buffer, request->size)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    sendRequestToThread(session_id);
}

/*
 * Reads read requests and sends it to respective
 * thread to process it
 */
void readProducer() {
    int session_id;
    ssize_t ret;
    if ((ret = readInt(&session_id)) < 0) {
        shutdownServer();
    }
    else if (ret == 0) {
        // There aren't write ends open
        return;
    }
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_READ;
    if ((ret = readInt(&request->fhandle)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    if ((ret = readSizeT(&request->size)) < 0) {
        trySendErrorToClient(sessions[session_id].fcli, session_id);
        shutdownServer();
    }
    else if (ret == 0)
        return;
    sendRequestToThread(session_id);
}

/*
 * Reads shutdown requests and sends it to respective
 * thread to process it
 */
void shutdownProducer() {
    int session_id;
    ssize_t ret;

    if ((ret = readInt(&session_id)) < 0) {
        shutdownServer();
    }
    else if (ret == 0) {
        // There aren't write ends open
        return;
    }
    r_args *request = get_request(session_id);
    request->op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    sendRequestToThread(session_id);

}

/*
 * Determine type of request and calls function to produce
 * request for thread
 */
void processRequest(char *char_opcode) {
    int op_code;
    char *ptr;
    op_code = (int) strtol(char_opcode, &ptr, 10);

    switch(op_code) {
        case TFS_OP_CODE_MOUNT :
            mountProducer();
            break;
        case TFS_OP_CODE_UNMOUNT :
            unmountProducer();
            break;
        case TFS_OP_CODE_OPEN :
            openProducer();
            break;
        case TFS_OP_CODE_CLOSE :
            closeProducer();
            break;
        case TFS_OP_CODE_WRITE :
            writeProducer();
            break;
        case TFS_OP_CODE_READ :
            readProducer();
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED :
            shutdownProducer();
            break;
        default:
            return;
    }

    return;
}


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    char opcode[1];

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

    // Open server pipe
    do {
        fserv = open(pipename, O_RDONLY);
    } while(fserv == -1 && errno == EINTR);
        
    if (fserv == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
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
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            shutdownServer();
        }
        processRequest(opcode);
    }

    return 0;
}