/***********************************************************************
 * @file      writer.c
 * @brief     Overwrites a string in the filepath specified.
 *
 * @authors   Sundar Krishnakumar
 *
 * @assignment ecen5713-assignment2

 */

#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>
#include <syslog.h>


#define RETRY_MAX 5

/***********************************************************************
 * @name      my_writer()
 * @brief     Overwrites a string in the filepath specified.
 *
 * @param     [char *] writefile
 * @param     [char *] writestr
 *
 * @return int
 *
 */
int my_writer(char *writefile, char *writestr)
{
    int fd = -1;
    int len = 0;
    int rc = -1;
    int retries = 0;

	fd = open(writefile, O_WRONLY | O_CREAT, 0644);
	if ( fd < 0 ) 
	{
		syslog(LOG_ERR, "Error with open(), errno is %d (%s)", errno, strerror(errno));
		return 1;
	}

	len = strlen(writestr);
	
    while ( (rc < len) && (retries < RETRY_MAX) )
    {
        lseek(fd, 0, SEEK_SET);

        syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	    rc = write(fd, writestr, len);

        if (rc < 0)
        { 

            syslog(LOG_ERR, "Error with write(), errno is %d (%s)", errno, strerror(errno));
            return 1;
        }

        retries++;

    }

    // check if max retries reached
    if ( retries == RETRY_MAX )
    {

        syslog(LOG_ERR, "Maximum no. of reties %u reached for write()", retries);
        return 1;

    }

	close(fd);	

    // exit with success
	return 0;

}

int main(int argc, char** argv)
{

    // Setup syslog. Uses LOG_USER facility.
    openlog(NULL, 0, LOG_USER);

    // Command line argument validation.
    // Results in message in /var/log/syslog
    if ( 3 == argc )
    {

        return my_writer(argv[1], argv[2]);

    }
    else
    {

        syslog(LOG_ERR, "Invalid no. of arguments: %u. Expected 2", argc - 1);
        syslog(LOG_ERR, "arg1: A full path to the file (including filename)");
        syslog(LOG_ERR, "arg2: Text string which will be written within this file");

        return 1;

    }


    closelog();

    return 0;
}