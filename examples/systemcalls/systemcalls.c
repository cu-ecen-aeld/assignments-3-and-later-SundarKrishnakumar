#include "systemcalls.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

    int status;

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/
    if (cmd != NULL)
    {

        status = system(cmd);

        if (status < 0)
        {

            perror("system");
            return false;

        }
        else
        {

            if (WIFEXITED(status))
            {
               if (WEXITSTATUS(status) == 0)
                {
                    // do nothing here
                }
                else
                {
                    return false;
                }
            }
            else
            {
                
                return false;
            }

        }

    }
    else
    {

        return false;

    }

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{

    pid_t pid;
    int status;


    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *   
*/

    pid = fork();
    
    // If fork() returns -1 no child process is created.
    if (pid == -1) // inside parent process
    {
        va_end(args);
        return false;
    }
    else if (pid == 0) // Inside child process
    {
        
        execv(command[0], command);
        exit(-1);

    }
    else if (pid > 0) // Inside parent process    
    {
        

        // Parent waits on the child
        if (waitpid(pid, &status, 0) == -1)
        {
            return false;
        }
        else
        {
            if (WIFEXITED(status))
            {

                if (WEXITSTATUS(status) == 0)
                {
                    // do nothing here
                }
                else
                {
                    return false;
                }
            }
            else
            {

                return false;
            }

        } 


    } 

    va_end(args);
    return true;
    
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    pid_t pid;
    int fd;
    int rc;
    int status;

    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *   
*/


    if (outputfile != NULL)
    {
        fd = open(outputfile, O_WRONLY|O_CREAT, 0644);

        if (fd < 0)
        {
            perror("open");
            return false;
        }


    }
    else
    {
        return false;

    }

    

    pid = fork();
    
    // If fork() returns -1 no child process is created.
    if (pid == -1) // inside parent process
    {
        close(fd);
        va_end(args);
        return false;
    }
    else if (pid == 0) // Inside child process
    {
        // redirect STD_OUT to the file we opened.
        rc = dup2(fd, STDOUT_FILENO);

        if (rc < 0)
        {
            perror("dup2");
            return false;
        }
       
        execv(command[0], command);
        exit(-1);


    }
    else if (pid > 0) // Inside parent process    
    {
        
        close(fd);

        // Parent waits on the child
        if (waitpid(pid, &status, 0) == -1)
        {
            return false;
        }
        else
        {
            if (WIFEXITED(status))
            {

                if (WEXITSTATUS(status) == 0)
                {
                    // do nothing here
                }
                else
                {
                    return false;
                }
            }
            else
            {

                return false;
            }

        } 

    }  

    va_end(args);
    return true;


}
