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

#define PORT 9000
#define MAX_CONNECTS 5
#define FILE_NAME "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 100
#define LINE_SZ_TABLE_MAX 10000


// global variables
struct sockaddr_in server_addr, client_addr;
socklen_t addrlen;
int server_fd, client_fd, fd;
sigset_t intmask;
char buffer[BUFFER_SIZE];

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

    syslog(LOG_DEBUG, "Caught signal, exiting");

    close(fd);	
	close(server_fd);
    closelog();

    // Delete file
    if(remove(FILE_NAME) < 0) 
    {
        syslog(LOG_DEBUG, "Unable to delete the file at %s", FILE_NAME);
    }

    exit(0);
}

int main(int argc, char *argv[])
{

    // local variables
    pid_t sid = 0, pid = 0;
    int long_index = 0;
    int opt = 0;
    
    // Setup syslog. Uses LOG_USER facility.
    openlog(NULL, 0, LOG_USER);


    // Register sig handler
    //set up alarm handler
    signal(SIGTERM, exit_handler);

    //set up signint handler
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

    if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGINT) == -1) ||
        (sigaddset(&intmask, SIGTERM) == -1))
    {
        
        perror("Failed to initialize the signal mask");
        return -1;

    }

    // Create socket here
    // AF_INET - Network not a unix socket
    // SOCK_STREAM - Reliable, bidrectional TCP
    // 0 - Use underlying protocol
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) 
    {
        perror("While creating socket");
        return -1;
    }	

    memset(&server_addr, 0, sizeof(server_addr  ));  
    server_addr.sin_family = AF_INET; // IPv4
	server_addr.sin_port = htons(PORT); // hton converts to network byte order
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);


 	if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("While binding to the port");
		return -1;
	}

    // Convert to deamon here
    // fork and make child deamon and kill the parent

    pid = fork();

    if (pid < 0)
    {
        printf("Fork failed\n");
        return -1;
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
        perror("While binding to the port");
        return -1;

    }

    // Change the current working directory to root.
    chdir("/");

    // Close stdin. stdout and stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);    


	// MAX_CONNECTS - queue size of pending connections
	if(listen(server_fd, MAX_CONNECTS) < 0)
	{
		perror("While listening");
		return -1;
	}

    // Creating output file
    fd = open(FILE_NAME, O_RDWR | O_CREAT, 0644);
	if ( fd < 0 ) 
	{
		syslog(LOG_ERR, "Error with open(), errno is %d (%s)", errno, strerror(errno));
		return -1;
	}    
 



	while(1)
	{

        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        
        if (client_fd < 0)
        {
            perror("While accepting client");
            return -1;
        } 

        // Block the signals SIGINT and SIGTERM here
        if (sigprocmask(SIG_BLOCK, &intmask, NULL) < 0)
        {
            break;
        }
        
        char s[INET6_ADDRSTRLEN];  

        inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        syslog(LOG_DEBUG,"Accepted connection from %s", s);   

        // TODO : handle failure ret values

		int read_size = 0;
		long write_size = 0;
        bool terminate = false;
        char *dyn_buffer1 = NULL;
        char *dyn_buffer2 = NULL;
        long size = BUFFER_SIZE;
        long cur_size = 0;
        int multiplier = 2;
        long curr_pos = 0;

        dyn_buffer1 = (char *)malloc(sizeof(char) * BUFFER_SIZE);


        do 
        {
            read_size = recv(client_fd, buffer, sizeof(buffer), 0);

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
                
            }

            memcpy(&dyn_buffer1[cur_size], buffer, read_size);
            cur_size += read_size;
                

  

        } while (terminate == false);

        // write to file
        write_size = write(fd, dyn_buffer1, cur_size);
        lin_sz_table[line_sz_index++] = cur_size;
     
        // TODO: send packet by packet i.e line by line
        
        curr_pos = lseek(fd, 0, SEEK_CUR);
        lseek(fd, 0, SEEK_SET);

        for (int i = 0; i < line_sz_index; i++)
        {
            dyn_buffer2 = (char *)malloc(sizeof(char) * lin_sz_table[i]);
            read(fd, dyn_buffer2, lin_sz_table[i]);
            send(client_fd, dyn_buffer2, lin_sz_table[i], 0);
            free(dyn_buffer2);
        }

        free(dyn_buffer1);     
        lseek(fd, curr_pos, SEEK_SET);

	
        close(client_fd);
        syslog(LOG_DEBUG,"Closed connection from %s", s);  

        // Unblock the signals SIGINT and SIGTERM here
        if (sigprocmask(SIG_UNBLOCK, &intmask, NULL) < 0)
        {
            break;
        }

	}

	close(fd);	
	close(server_fd);
    closelog();


   if(remove(FILE_NAME) < 0) 
   {
      syslog(LOG_DEBUG, "Unable to delete the file at %s", FILE_NAME);
   }

    return 0;
}