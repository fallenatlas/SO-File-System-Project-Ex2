#include "operations.h"

#define OP_CODE_SIZE 1
#define SIZE_REQUEST 80

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    int r;
    FILE *fserv;
    char *buf;
    char *request;
    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) {
        exit(1);
    }

    if ((fserv = fopen(pipename, "r")) == NULL) {
        exit(1);
    }

    if (tfs_init() == -1) {
        exit(1);
    }

    for(;;) {
        r = fread(buf, sizeof(char), OP_CODE_SIZE, fserv);
        //r = fread(request, sizeof(char), SIZE_REQUEST, fserv);
        if (r <= 0) {
            break;
        }
        processRequest(buf);
    }

    fclose(fserv);
    unlink(pipename);
    return 0;
}

int processRequest(char *buf) {
    long op_code;
    char *ptr;
    op_code = strtol(buf, &ptr, 10);

    switch(op_code) {
        case TFS_OP_CODE_MOUNT :
            // read remaining input and send to thread
            // either send result to client here of put it in buf and send it in the main loop
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
    }

    return 0;
}