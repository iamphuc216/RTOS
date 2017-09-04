/*
	main.c
	Assignment 2
	Real-time Operating System
	Smit Patel 11819164  
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>




#define BUF_SIZE 1024 // Read 1024 bytes


typedef struct {
    int n_buf;	//-1 at completion 
    char buf[BUF_SIZE];	//1kb buffer
} char_buffer;
	
	// Structures for Threads
typedef struct {
    
    sem_t *sem_read_file;
    sem_t *sem_read_pipe;
    int *fd_write;
    FILE *file;
} Struct_A;

typedef struct {
    
    sem_t *sem_read_pipe;
    sem_t *sem_read_shared;
    int *fd_read;
    char_buffer *shared;
} Struct_B;

typedef struct {
    
    sem_t *sem_read_shared;
    sem_t *sem_read_file;
    char_buffer *shared;
    FILE *file;
} Struct_C;

// routine for Semaphore
int handle_sem_wait(sem_t *sem, char thread_identifier ) {
    if (sem_wait(sem)) {
        printf("Thread %c. Terminated\n", thread_identifier );
        return -1;
    }
    return 0;
}

int handle_sem_post(sem_t *sem, char thread_identifier ) {
    if (sem_post(sem)) {
        printf("Thread %c. Terminated\n", thread_identifier );
        return -1;
    }
    return 0;
}

sem_t *named_sem_open(char const * const name) {

    sem_unlink(name);	//Unlinking any existing Semaphore is necessary to create new one
    
    sem_t *sem = sem_open(name, O_CREAT | O_EXCL, S_IRWXU, 0);
    if (sem == SEM_FAILED) {
        return NULL;
    }
    return sem;
}



void threadA_subroutine(Struct_A * arg) {
    //Read file to pipe
    const char letter = 'A';
    char buf[BUF_SIZE];
    fpos_t old_pos = 0;
    fpos_t new_pos = 0;
    for (;;) {
        if (handle_sem_wait(arg->sem_read_file, letter)) {
            break;
        }
        
        if (fgets(buf, sizeof(buf), arg->file) == NULL) {
            /*Print an error message if fgets returns
             NULL and it is not the end of the file
             */
            if (!feof(arg->file)) {
                printf("File reading error.Thread A terminated. %d", ferror(arg->file));
            }
            break;
        }
        if (fgetpos(arg->file, &new_pos)) {
            printf("file Position error.Thread A terminated.\n");
            break;
        }
        size_t read = new_pos - old_pos;
        old_pos = new_pos;
        if (write(*arg->fd_write, buf, read) < 0) {
            printf("Writing to the pipe error. Thread A terminated.\n");
            break;
        }
        if (handle_sem_post(arg->sem_read_pipe, letter)) {
            break;
        }
    }
    
  
    if (!close(*arg->fd_write)) {	//Closing file for End of File flag trigger
        *arg->fd_write = 0;
    } else {
        printf("Pipe closing error at Thread A teardown.\n");
    }
    handle_sem_post(arg->sem_read_pipe, letter);
}

void threadB_subroutine(Struct_B * arg) {
    const char letter = 'B';
    int nread;	//pipe to shared buffer
    for (;;) {
        if (handle_sem_wait(arg->sem_read_pipe, letter)) {
            break;
        }

        nread = read(*(arg->fd_read), arg->shared->buf, BUF_SIZE);
        if (nread == 0) {
            break;
        } else if (nread < 0) {
            printf("Can't read from pipe. Thread B terminated.\n");
            break;
        }
        arg->shared->n_buf = nread;
        if (handle_sem_post(arg->sem_read_shared, letter)) {
            break;
        }
    }
    
   
    if(!close(*arg->fd_read)) {	//file descripter closed.
        *arg->fd_read = 0;
    }
    
    
    arg->shared->n_buf = -1;	//Buffer reset
    handle_sem_post(arg->sem_read_shared, letter);
}

void threadC_subroutine(Struct_C * arg) {
    const char letter = 'C';
    const size_t s_detection = sizeof("end_header") - 1;	//Differentiating File header and Content region
    for (;;) {	//Reading shared budder to file.
        if (handle_sem_wait(arg->sem_read_shared, letter)) {
            break;
        }
        
        if (arg->shared->n_buf < 0) {
            break;
        }
        static int content_region = 0;
        if (content_region) {
            fprintf(arg->file, "%.*s", (int)arg->shared->n_buf, arg->shared->buf);
        }
        if (!memcmp(arg->shared->buf, "end_header", s_detection)) {
            content_region = 1;
        }
        if (handle_sem_post(arg->sem_read_file, letter)) {
            break;
        }
    }
}


// main function
int main(int argc, const char* argv[]) {
    const char letter = 'M';
    int result = -1;
    
    
    //Make pipe
    int fd[2];
    if (pipe(fd) < 0) 	// Nothing has been done.
        printf("Unable to create pipe.\n");
        

    //Open the data.txt file for reading
    FILE *data_file = fopen("data.txt", "r");
    if (data_file == NULL) {
        printf("Error while opening data.txt for reading.\n");
        goto data_open_failed;
    }
    
    //Open the src.txt file for writing
    FILE *src_file = fopen("src.txt", "w");
    if (src_file == NULL) {
        printf("Error while opening src.txt for writing.\n" );
        goto src_open_failed;
    }
    
  	// semaphore set-up
    sem_t *write; //writing to pipe
    sem_t *read; //reading from pipe
    sem_t *justify; //justify output
    
    if (!(write = named_sem_open("/sem_read_file"))) {
        printf("Unable to create read semaphore.\n");
        goto open_write_sem_failed;
    }
    
    if (!(read = named_sem_open("/sem_read_pipe"))) {
        printf("Unable to create write semaphore.\n");
        goto open_read_sem_failed;
    }
    
    if (!(justify = named_sem_open("/sem_read_shared"))) {
        printf("Unable to create read semaphore.\n");
        if (sem_unlink("/sem_read_pipe")) 
        	printf("Error while unlinking the read semaphore.\n");;
    }
    
    
    //exit flag allows us to prematurely terminate threads.
    volatile int exit_flag = 0;
    
    char_buffer shared = {0};	//Shared buffer set-up
    
    Struct_A a = { write, read, &fd[1], data_file};
    Struct_B b = { read, justify, &fd[0], &shared};
    Struct_C c = { justify, write, &shared, src_file};
    
    //Create threads
    pthread_t thread_a, thread_b, thread_c;
    
    if(pthread_create(&thread_a, NULL, (void *)threadA_subroutine, (void *)&a) != 0)
    {
        perror("pthread_create");

    }
    
    if(pthread_create(&thread_b, NULL, (void *)threadB_subroutine, (void *)&b) != 0)
    {
        perror("pthread_create");
        exit_flag = 1;
        sem_post(write);

    }
    
    if(pthread_create(&thread_c, NULL, (void *)threadC_subroutine, (void *)&c) != 0)
    {
        perror("pthread_create");
        exit_flag = 1;
        sem_post(read);
        sem_post(write);

    }
    
   	// check for ordeal 
    handle_sem_post(write, letter);  // this concept was suggested by Robin Wohlers
    pthread_join(thread_c, NULL);
    result = 0;

    
open_read_sem_failed://write semaphore has been created
    if (sem_unlink("/sem_read_file")) {
        printf("Error while unlinking the justify semaphore.\n");
    }
    
open_write_sem_failed://src.txt has been opened for writing
    if(fclose(src_file)) {
        printf("Error while closing the src file.\n");
    }
    
src_open_failed://data.txt has been opened successfully
    if(fclose(data_file)) {
        printf("Error while closing the data file.\n");
    }
    
data_open_failed://pipe has been opened.

    // closing the pipe on read side if open and set file descriptor to 0.
    if (fd[0]) {
        if (close(fd[0])) {
            printf("Error closing the pipe on read side.\n");
        }
    }
    // closing the pipe on write side if open and set file descriptor to 1.
    if (fd[1]) {
        if (close(fd[1])) {
            printf("Error closing the pipe on write side.\n");
        }
    }
    return result;
}
