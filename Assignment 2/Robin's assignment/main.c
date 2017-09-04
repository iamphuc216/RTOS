//
//  main.c
//  ass2
//
//  Created by Robin Wohlers Reichel on 24/4/17.
//  Copyright © 2017 Robin Wohlers-Reichel. All rights reserved.
//

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#pragma mark - Constant definitions

//Buffer must be greater in length than the header detection char array
#define BUFFER_SIZE 1024 /* read 1024 bytes at a time */
#define HEADER_DETECTION "end_header"

#define DATA_FILENAME "data.txt"
#define SRC_FILENAME "src.txt"

#define SEM_NAME_READ_FILE "/sem_read_file"
#define SEM_NAME_READ_PIPE "/sem_read_pipe"
#define SEM_NAME_READ_SHARED "/sem_read_shared"

#pragma mark - Type definitions

typedef struct {
    ssize_t n_buf; //-1 when complete. Belt and suspenders!
    char buf[BUFFER_SIZE];
} char_buffer;

typedef struct {
    volatile int *exit_flag;
    sem_t *sem_read_file;
    sem_t *sem_read_pipe;
    int *fd_write;
    FILE *file;
} arg_struct_a;

typedef struct {
    volatile int *exit_flag;
    sem_t *sem_read_pipe;
    sem_t *sem_read_shared;
    int *fd_read;
    char_buffer *shared;
} arg_struct_b;

typedef struct {
    volatile int *exit_flag;
    sem_t *sem_read_shared;
    sem_t *sem_read_file;
    char_buffer *shared;
    FILE *file;
} arg_struct_c;

/*
 data.txt -> thread A -> pipe -> thread B-> **?** -> thread C -> src.txt
 */

#pragma mark - Convenience functions

int handle_exit_flag(volatile int *flag, char thread_letter) {
    if (*flag) {
        printf("Exit flag set. Aborting thread %c.\n", thread_letter);
        return -1;
    }
    return 0;
}

int handle_sem_wait(sem_t *sem, char thread_letter) {
    if (sem_wait(sem)) {
        printf("Error while waiting on semaphore. Aborting thread %c. %d.\n", thread_letter, errno);
        return -1;
    }
    return 0;
}

int handle_sem_post(sem_t *sem, char thread_letter) {
    if (sem_post(sem)) {
        printf("Error while signaling semaphore. Aborting thread %c. %d.\n", thread_letter, errno);
        return -1;
    }
    return 0;
}

sem_t *named_sem_open(char const * const name) {
    /*
     As we are using named semaphores for
     maximum portability, we should first unlink any
     existing semaphores.
     */
    sem_unlink(name);
    
    sem_t *sem = sem_open(name, O_CREAT | O_EXCL, S_IRWXU, 0);
    if (sem == SEM_FAILED) {
        return NULL;
    }
    return sem;
}

#pragma mark - Thread functions

void thread_fn_a(arg_struct_a * p) {
    //Read file to pipe
    const char letter = 'A';
    char buf[BUFFER_SIZE];
    fpos_t old_pos = 0;
    fpos_t new_pos = 0;
    for (;;) {
        if (handle_sem_wait(p->sem_read_file, letter)) {
            break;
        }
        if (handle_exit_flag(p->exit_flag, letter)) {
            break;
        }
        if (fgets(buf, sizeof(buf), p->file) == NULL) {
            /*Print an error message if fgets returns
             NULL and it is not the end of the file
             */
            if (!feof(p->file)) {
                printf("Error reading from file. Aborting thread A. %d", ferror(p->file));
            }
            break;
        }
        if (fgetpos(p->file, &new_pos)) {
            printf("Error while getting the position in the file. Aborting thread A. %d.\n", errno);
            break;
        }
        size_t read = new_pos - old_pos;
        old_pos = new_pos;
        if (write(*p->fd_write, buf, read) < 0) {
            printf("Error while writing to the pipe. Aborting thread A. %d.\n", errno);
            break;
        }
        if (handle_sem_post(p->sem_read_pipe, letter)) {
            break;
        }
    }
    
    /*
     Close the pipe to send an EOF to the next thread.
     */
    if (!close(*p->fd_write)) {
        //Successfully closed the file descriptor
        *p->fd_write = 0;
    } else {
        printf("Error while closing read end of pipe during thread A teardown. %d.\n", errno);
    }
    handle_sem_post(p->sem_read_pipe, letter);
}

void thread_fn_b(arg_struct_b * p) {
    const char letter = 'B';
    //Read pipe to shared
    ssize_t nread;
    for (;;) {
        if (handle_sem_wait(p->sem_read_pipe, letter)) {
            break;
        }
        if (handle_exit_flag(p->exit_flag, letter)) {
            break;
        }
        nread = read(*(p->fd_read), p->shared->buf, BUFFER_SIZE);
        if (nread == 0) {
            break;
        } else if (nread < 0) {
            printf("Error while reading from pipe. Aborting thread B %d.\n", errno);
            break;
        }
        p->shared->n_buf = nread;
        if (handle_sem_post(p->sem_read_shared, letter)) {
            break;
        }
    }
    
    /*
     Close the read end of the pipe.
     */
    if(!close(*p->fd_read)) {
        //Successfully closed the file descriptor
        *p->fd_read = 0;
    }
    
    /*
     Set the number of bytes in the buffer to -1 to
     represent the end of the data.
     */
    p->shared->n_buf = -1;
    handle_sem_post(p->sem_read_shared, letter);
}

void thread_fn_c(arg_struct_c * p) {
    const char letter = 'C';
    const size_t s_detection = sizeof(HEADER_DETECTION) - 1;
    //Read shared to file
    for (;;) {
        if (handle_sem_wait(p->sem_read_shared, letter)) {
            break;
        }
        if (handle_exit_flag(p->exit_flag, 'A')) {
            break;
        }
        if (p->shared->n_buf < 0) {
            break;
        }
        static int past_header = 0;
        if (past_header) {
            fprintf(p->file, "%.*s", (int)p->shared->n_buf, p->shared->buf);
        }
        if (!memcmp(p->shared->buf, HEADER_DETECTION, s_detection)) {
            past_header = 1;
        }
        if (handle_sem_post(p->sem_read_file, letter)) {
            break;
        }
    }
}

#pragma mark - main

int main(int argc, const char* argv[]) {
    const char letter = 'M';
    int result = -1;
    
    /*
     Makey pipe
     */
    int fd[2];
    if (pipe(fd) < 0) {
        printf("Unable to create pipe. %d\n", errno);
        goto make_pipe_failed;
    }
    
    /*
     Opening the files
     */
    //Open the data.txt file for reading
    FILE *data_file = fopen(DATA_FILENAME, "r");
    if (data_file == NULL) {
        printf("Error while opening %s for reading. %d.\n", DATA_FILENAME, errno);
        goto data_open_failed;
    }
    
    //Open the src.txt file to use as a destination
    FILE *src_file = fopen(SRC_FILENAME, "w");
    if (src_file == NULL) {
        printf("Error while opening %s for writing. %d.\n", SRC_FILENAME, errno);
        goto src_open_failed;
    }
    
    /*
     Setting up semaphores
     */
    sem_t *write; //semaphore for writing to pipe
    sem_t *read; //semaphore for reading from pipe
    sem_t *justify; //semphore to justify output
    
    if (!(write = named_sem_open(SEM_NAME_READ_FILE))) {
        printf("Unable to create read semaphore. %d.\n", errno);
        goto open_write_sem_failed;
    }
    
    if (!(read = named_sem_open(SEM_NAME_READ_PIPE))) {
        printf("Unable to create write semaphore. %d.\n", errno);
        goto open_read_sem_failed;
    }
    
    if (!(justify = named_sem_open(SEM_NAME_READ_SHARED))) {
        printf("Unable to create read semaphore. %d.\n", errno);
        goto open_justify_sem_failed;
    }
    
    /*
     Set up shared data
     */
    
    //exit flag allows us to prematurely terminate threads.
    volatile int exit_flag = 0;
    
    char_buffer shared = {0};
    
    arg_struct_a a = {&exit_flag, write, read, &fd[1], data_file};
    arg_struct_b b = {&exit_flag, read, justify, &fd[0], &shared};
    arg_struct_c c = {&exit_flag, justify, write, &shared, src_file};
    
    /*
     Create threads
     */
    pthread_t thread_a, thread_b, thread_c;
    
    if(pthread_create(&thread_a, NULL, (void *)thread_fn_a, (void *)&a) != 0)
    {
        perror("pthread_create");
        goto create_thread_a_failed;
    }
    
    if(pthread_create(&thread_b, NULL, (void *)thread_fn_b, (void *)&b) != 0)
    {
        perror("pthread_create");
        exit_flag = 1;
        sem_post(write);
        goto create_thread_b_failed;
    }
    
    if(pthread_create(&thread_c, NULL, (void *)thread_fn_c, (void *)&c) != 0)
    {
        perror("pthread_create");
        exit_flag = 1;
        sem_post(read);
        sem_post(write);
        goto create_thread_c_failed;
    }
    
    /*
     Kick-off the whole ordeal
     */
    handle_sem_post(write, letter);
    
    /*
     Wait for the ordeal to finish
     */
    pthread_join(thread_c, NULL);
    
    /*
     Seems that all went well
     */
    result = 0;
    goto success_normal_teardown;

    /*
     Teardown section.
     */
create_thread_c_failed://thread B has been created
    pthread_join(thread_b, NULL);
    
create_thread_b_failed://thread A has been created
    pthread_join(thread_a, NULL);
    
success_normal_teardown:
    
create_thread_a_failed://justify semaphore has been created
    if (sem_unlink(SEM_NAME_READ_SHARED)) {
        printf("Error while unlinking the write semaphore. %d.\n", errno);
    }
    
open_justify_sem_failed://read semaphore has been created
    if (sem_unlink(SEM_NAME_READ_PIPE)) {
        printf("Error while unlinking the read semaphore. %d.\n", errno);
    }
    
open_read_sem_failed://write semaphore has been created
    if (sem_unlink(SEM_NAME_READ_FILE)) {
        printf("Error while unlinking the justify semaphore. %d.\n", errno);
    }
    
open_write_sem_failed://src.txt has been opened for writing
    if(fclose(src_file)) {
        printf("Error while closing the src file. %d.\n", errno);
    }
    
src_open_failed://data.txt has been opened successfully
    if(fclose(data_file)) {
        printf("Error while closing the data file. %d.\n", errno);
    }
    
data_open_failed://pipe has been opened.
    /*
     If the pipes have been closed then the thread sets
     the file descriptor to 0. Otherwise we must close them.
     */
    if (fd[0]) {
        if (close(fd[0])) {
            printf("Error while closing the read end of the pipe. %d\n", errno);
        }
    }
    if (fd[1]) {
        if (close(fd[1])) {
            printf("Error while closing the write end of the pipe. %d\n", errno);
        }
    }
    
make_pipe_failed://nothing has been done yet
    
    
    return result;
}
