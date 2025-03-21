#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    // open log
    openlog(NULL, 0, LOG_USER);

    // check num args
    if (argc != 3)
    {
        // add LOG_ERR to syslog
        syslog(LOG_ERR, "Invalid number of arguments: ./writer <filedir> <filestr>\n");
        // be sure to close log before exiting
        closelog();
        return 1;
    }
    
    // initialize args to variables
    char *writefile = argv[1];
    char *writestr = argv[2];
    
    // open file and check if file exists
    FILE *file = fopen(writefile, "w");
    if (file == NULL)
    {
        // add LOG_ERR to syslog
        syslog(LOG_ERR, "Failed to open file: %s\n", writefile);
        closelog();
        return 1;
    }
    
    // write to file
    fprintf(file, "%s\n", writestr);
    // close file
    fclose(file);
    
    // add log entry for successful writing to file
    syslog(LOG_DEBUG, "Wrote %s to %s\n", writestr, writefile);
    closelog();
    
    return 0;
}
