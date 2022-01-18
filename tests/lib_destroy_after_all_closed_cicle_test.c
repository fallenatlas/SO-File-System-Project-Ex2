#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/*  Simple test to check whether the implementation of
    tfs_destroy_after_all_closed is correct.
    Note: This test uses TecnicoFS as a library, not
    as a standalone server.
    We recommend trying more elaborate tests of tfs_destroy_after_all_closed.
    Also, we suggest trying out a similar test once the
    client-server version is ready (calling the tfs_shutdown_after_all_closed 
    operation).
*/
#define SIZE 40

int open_files = 0;
int f;

void *fn_thread(void *arg) {
    (void)
        arg; /* Since arg is not used, this line prevents a compiler warning */
    char buffer[SIZE];
    memset(buffer, 'A', SIZE);
    
    for (int i=0; i<10; i++){
        f = tfs_open("/f1", TFS_O_CREAT);
        if (f != -1) {
            printf("Open file: %d\n", i);
            ++open_files;
        }
        else {
            printf("Failed open file: %d\n", i);
        }
        tfs_write(f, buffer, SIZE);
        /* set *before* closing the file, so that it is set before
           tfs_destroy_after_all_close returns in the main thread
        */
        --open_files;
        assert(tfs_close(f) != -1);
    }
    return NULL;
}


int main() {

    assert(tfs_init() != -1);

    pthread_t t;
    
    assert(f != -1);

    assert(pthread_create(&t, NULL, fn_thread, NULL) == 0);
    assert(tfs_destroy_after_all_closed() != -1);
    printf("Tfs_destroy\n");
    assert(open_files == 0);
    pthread_join(t, NULL);
    // No need to join thread
    printf("Successful test.\n");

    return 0;
}
