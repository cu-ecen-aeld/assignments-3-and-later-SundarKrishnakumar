// Author: Sundar Krishnakumar
// file: aesdsocket.c
// brief: stream socket server
// Reference/Re-use: https://beej.us/guide/bgnet/html/
//            https://www.educative.io/edpresso/how-to-implement-tcp-sockets-in-c
//            https://codingfreak.blogspot.com/2009/10/signals-in-linux-blocking-signals.html
//            https://linuxprograms.wordpress.com/2012/06/22/c-getopt_long-example-accessing-command-line-arguments/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <getopt.h>
#include <stdbool.h>
#include "queue.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>


#define PORT 9000
#define MAX_CONNECTS 20
#define FILE_NAME "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 100
#define LINE_SZ_TABLE_MAX 10000
#define TIME_BUFFER_SIZE 150
#define PRINT_BUFFER_SIZE 200


// global variables
struct sockaddr_in server_addr, client_addr;
socklen_t addrlen;
int server_fd, fd;


typedef struct thread_param_s thread_param_t;
struct  thread_param_s
{
    int client_fd;
    sigset_t intmask;  
    bool complete;  
    pthread_t thread_id;
    pthread_mutex_t *mutex_lock;

};

// Setup linkedlist
typedef struct slist_data_s slist_data_t;
struct slist_data_s {

    thread_param_t thread_param;
    SLIST_ENTRY(slist_data_s) entries;
};

// mutex lock variable
pthread_mutex_t mutex_lock;

slist_data_t *datap = NULL;
slist_data_t *tmp = NULL; 
SLIST_HEAD(slisthead, slist_data_s) head;

long lin_sz_table[LINE_SZ_TABLE_MAX];
long line_sz_index = 0;
bool is_daemon = false;

struct option long_options[] = {
    {"daemon", no_argument, 0, 'a'},
    {0, 0, 0, 0}
};

// get sockaddr, IPv4 or IPv6
// Code source: https://beej.us/guide/bgnet/html/
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// graceful exit
void exit_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");

        if(0 != shutdown(server_fd, SHUT_RDWR))
        {
            syslog(LOG_DEBUG, "shutdown failed");
        }
    }

}

void timer_handler(int signum)
{

    int ret;
    int write_size;

    time_t rawtime;
    struct tm *info;
    char time_buffer[TIME_BUFFER_SIZE];
    char print_buffer[PRINT_BUFFER_SIZE];

    time( &rawtime );
    info = localtime( &rawtime );
    strftime(time_buffer, TIME_BUFFER_SIZE, "%Y/%m/%d-%H:%M:%S", info);
    sprintf(print_buffer, "timestamp:%s\n", time_buffer);  


    write_size = strlen(print_buffer);

    if ( 0 != pthread_mutex_lock(&mutex_lock))
    {
        syslog(LOG_ERR, "While acquiring the lock");
      
        goto timer_handler_exit_segment;
    }

    // write to file
    ret = write(fd, print_buffer, write_size);
    if (ret < 0)
    {
        syslog(LOG_ERR, "While writing to file");
        goto timer_handler_exit_segment;
        
    }

    // Put a check for 10001st packet
    lin_sz_table[line_sz_index] = write_size;
    line_sz_index = (line_sz_index + 1) % LINE_SZ_TABLE_MAX;

    if ( 0 != pthread_mutex_unlock(&mutex_lock))
    {
        syslog(LOG_ERR, "While releasing the lock");
        goto timer_handler_exit_segment;
    }    


timer_handler_exit_segment:
    return;

}


void* threadfunc(void* thread_param)
{
    
    thread_param_t *local_param = (thread_param_t *)thread_param;
    sigset_t intmask = local_param->intmask;
    int client_fd = local_param->client_fd;
    pthread_mutex_t *lock = local_param->mutex_lock;

    char buffer[BUFFER_SIZE];

    int read_size = 0;
    int ret = 0;
    bool terminate = false;
    char *dyn_buffer1 = NULL;
    char *dyn_buffer2 = NULL;
    long size = BUFFER_SIZE;
    long cur_size = 0;
    int multiplier = 2;
    long curr_pos = 0;
    bool error_in_thread = false;

    // Block the signals SIGINT and SIGTERM here
    if (sigprocmask(SIG_BLOCK, &intmask, NULL) < 0)
    {
        error_in_thread = true;
        goto thread_exit_segment;
    }

    char s[INET6_ADDRSTRLEN];  

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
    syslog(LOG_DEBUG,"Accepted connection from %s", s);   
    

    dyn_buffer1 = (char *)malloc(sizeof(char) * BUFFER_SIZE);

    if (dyn_buffer1 == NULL)
    {
        syslog(LOG_ERR, "(dyn_buffer1) malloc returned NULL");
        error_in_thread = true;
        goto thread_exit_segment;
    }


    do 
    {
        read_size = recv(client_fd, buffer, sizeof(buffer), 0);

        if (read_size < 0)
        {
            syslog(LOG_ERR, "While receiving socket data");
            error_in_thread = true;
            goto thread_exit_segment;

        }

        if (!read_size || (strchr(buffer, '\n') != NULL)) terminate = true;

        // dyn_buffer1 full, must resize
        if ((size - cur_size) < read_size)
        {
            do
            {                                    
                size = BUFFER_SIZE * multiplier;
                multiplier++;

            } while ((size - cur_size) < read_size);

            dyn_buffer1 = (char *)realloc(dyn_buffer1, sizeof(char) * size);
            
            if (dyn_buffer1 == NULL)
            {
                syslog(LOG_ERR, "(dyn_buffer1) realloc returned NULL");
                error_in_thread = true;
                goto thread_exit_segment;
            }
            
        }

        memcpy(&dyn_buffer1[cur_size], buffer, read_size);
        cur_size += read_size;
            



    } while (terminate == false);

    if ( 0 != pthread_mutex_lock(lock))
    {
        syslog(LOG_ERR, "While acquiring the lock");
        error_in_thread = true;
        goto thread_exit_segment;
    }


    // write to file
    ret = write(fd, dyn_buffer1, cur_size);
    if (ret < 0)
    {
        syslog(LOG_ERR, "While writing to file");
        error_in_thread = true;
        goto thread_exit_segment;
        
    }

    // put a check for 10001st packet
    lin_sz_table[line_sz_index] = cur_size;
    line_sz_index = (line_sz_index + 1) % LINE_SZ_TABLE_MAX;

    // send packet by packet i.e line by line
    curr_pos = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);

    for (int i = 0; i < line_sz_index; i++)
    {
        dyn_buffer2 = (char *)malloc(sizeof(char) * lin_sz_table[i]);

        if (dyn_buffer2 == NULL)
        {
            syslog(LOG_ERR, "(dyn_buffer2) malloc returned NULL");
            error_in_thread = true;
            goto thread_exit_segment;
        }
        
        ret = read(fd, dyn_buffer2, lin_sz_table[i]);
        if (ret < 0)
        {
            syslog(LOG_ERR, "While reading from file");
            error_in_thread = true;
            goto thread_exit_segment;
            
        }

        ret = send(client_fd, dyn_buffer2, lin_sz_table[i], 0);
        if (ret < 0)
        {
            syslog(LOG_ERR, "While sending socket data");
            error_in_thread = true;
            goto thread_exit_segment;
            
        }        

        free(dyn_buffer2);
        dyn_buffer2 = NULL;
    }

    free(dyn_buffer1);  
    dyn_buffer1 = NULL;   
    lseek(fd, curr_pos, SEEK_SET);

    if ( 0 != pthread_mutex_unlock(lock))
    {
        syslog(LOG_ERR, "While releasing the lock");
        error_in_thread = true;
        goto thread_exit_segment;
    }


    close(client_fd);
    syslog(LOG_DEBUG,"Closed connection from %s", s);  

    // Unblock the signals SIGINT and SIGTERM here
    if (sigprocmask(SIG_UNBLOCK, &intmask, NULL) < 0)
    {
        error_in_thread = true;
        goto thread_exit_segment;
    }

thread_exit_segment:    
   
    if (error_in_thread == true)
    {
        if (dyn_buffer1 != NULL)
        {
            free(dyn_buffer1);
        }

        if (dyn_buffer2 != NULL)
        {
            free(dyn_buffer2);
        }

    }

    // set complete to false
    local_param->complete = true;

    return NULL;

}

int main(int argc, char *argv[])
{
    // local variables
    pid_t sid = 0, pid = 0;
    int long_index = 0;
    int opt = 0;
    bool error = false;
    int client_fd;

    // set up signint handler
    signal(SIGINT, exit_handler);
    
    while ((opt = getopt_long(argc, argv,"d", 
                   long_options, &long_index )) != -1) {
        switch (opt) {
            case 'd' : 
                is_daemon = true;
                break;
            default:
                break;
        }
    }  

    // Convert to deamon here
    // fork and make child deamon and kill the parent

    if (is_daemon == true)
    {

        pid = fork();

        if (pid < 0)
        {
            printf("Fork failed\n");
            error = true;
            goto exit_segment;
        }
        else if (pid > 0)
        {
            printf("Child process PID: %d \n", pid);
            exit(0);
        }

        // Inside child process
        //set new session
        sid = setsid();

        if(sid < 0)
        {
            syslog(LOG_ERR, "While starting new session in child");
            error = true;
            goto exit_segment;

        }

        // Change the current working directory to root.
        chdir("/");

        // Close stdin. stdout and stderr
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);  

    }      

 
    // Configure and start 10s timer
    signal(SIGALRM, timer_handler);
    struct itimerval timer;
    timer.it_value.tv_sec = 10;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 10;
    timer.it_interval.tv_usec = 0;
    if (setitimer (ITIMER_REAL, &timer, NULL) != 0)
    {

        syslog(LOG_ERR, "While setting 10s timer");
        error = true;
        goto exit_segment;
      
    }  

    // Setup syslog. Uses LOG_USER facility.
    openlog(NULL, 0, LOG_USER);

    // initialize head of linked list
    SLIST_INIT(&head);


    // Register sig handler
    // set up alarm handler
    signal(SIGTERM, exit_handler);  


    // initialize the lock
    if (pthread_mutex_init(&mutex_lock, NULL) != 0) 
    {
        syslog(LOG_ERR, "While intializing mutex");
        error = true;
        goto exit_segment;
    }  

    // Create socket here
    // AF_INET - Network not a unix socket
    // SOCK_STREAM - Reliable, bidrectional TCP
    // 0 - Use underlying protocol
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) 
    {
        syslog(LOG_ERR, "While creating socket");
        error = true;
        goto exit_segment;
    }	

    memset(&server_addr, 0, sizeof(server_addr  ));  
    server_addr.sin_family = AF_INET; // IPv4
	server_addr.sin_port = htons(PORT); // hton converts to network byte order
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int flag = 1;  
    if (0 != setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag))) {  
        syslog(LOG_ERR, "setsockopt fail"); 
        error = true;
        goto exit_segment;         
    }      


 	if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) < 0)
	{
		syslog(LOG_ERR, "While binding to the port");
        error = true;
        goto exit_segment;
	}


	// MAX_CONNECTS - queue size of pending connections
	if(listen(server_fd, MAX_CONNECTS) < 0)
	{
		syslog(LOG_ERR, "While listening");
        error = true;
        goto exit_segment;
	}

    // Creating output file
    // NOTE: fd is a global variable and is not supplied as param to each thread
    fd = open(FILE_NAME, O_RDWR | O_CREAT, 0644);
	if ( fd < 0 ) 
	{
		syslog(LOG_ERR, "Error with open(), errno is %d (%s)", errno, strerror(errno));
        error = true;
        goto exit_segment;
	}
        

	while(1)
	{ 
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);

        if (client_fd < 0)
        {
            syslog(LOG_ERR, "While accepting client");
            error = true;
            goto exit_segment;
        } 

        // Prepare thread local variable for new thread    
        // and enter the node in the list
        // check malloc error code
        datap = (slist_data_t *)malloc(sizeof(slist_data_t));
        if (datap == NULL)
        {
            syslog(LOG_ERR, "(datap) malloc returned NULL");
            error = true;
            goto exit_segment;
        }

        
        datap->thread_param.client_fd = client_fd;
        datap->thread_param.complete = false;  
        datap->thread_param.mutex_lock = &mutex_lock;

        if ((sigemptyset(&datap->thread_param.intmask) == -1) || 
            (sigaddset(&datap->thread_param.intmask, SIGINT) == -1) || 
                (sigaddset(&datap->thread_param.intmask, SIGTERM) == -1))
        {
            
            syslog(LOG_ERR, "Failed to initialize the signal mask");
            error = true;
            goto exit_segment;

        }

        SLIST_INSERT_HEAD(&head, datap, entries);    
        

        // create new thread here and handle failure
        if (0 != pthread_create(&(datap->thread_param.thread_id), NULL, &threadfunc, (void *) &(datap->thread_param)))
        {

            syslog(LOG_ERR, "Failed to create a new thread");
            error = true;
            goto exit_segment;            

        }

        // Check if any thread completed
        SLIST_FOREACH_SAFE(datap, &head, entries, tmp) 
        {

            if (datap->thread_param.complete == true)
            {
                if (pthread_join(datap->thread_param.thread_id,  NULL) != 0)
                {
                  
                    syslog(LOG_ERR, "pthread_join was not successfull on thread id: %d", 
                        datap->thread_param.complete);
                        
                    error = true;
                    goto exit_segment;

                }


                SLIST_REMOVE(&head, datap, slist_data_s, entries); 
                free(datap);
                datap = NULL;
            }        
            
        }

        
	}



exit_segment:

	close(fd);	
	close(server_fd);    
    
    // delete linked list
    SLIST_FOREACH_SAFE(datap, &head, entries, tmp) 
    {

        if (datap->thread_param.complete == true)
        {
            pthread_join(datap->thread_param.thread_id,  NULL);
        } 

        close(datap->thread_param.client_fd);

        SLIST_REMOVE(&head, datap, slist_data_s, entries); 
        free(datap);
        datap = NULL;
              
        
    }    

    // destroy lock
    pthread_mutex_destroy(&mutex_lock);

   if(remove(FILE_NAME) < 0) 
   {
      syslog(LOG_ERR, "Unable to delete the file at %s", FILE_NAME);
   }

   closelog();

   if (error == true)
   {
       return -1;
   }
   else
   {
       return 0;
   }

    
}