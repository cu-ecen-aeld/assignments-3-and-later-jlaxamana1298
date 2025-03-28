#include "systemcalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int system_return_val = 0;
    system_return_val = system(cmd);

    // if return val = 0 return true
    if (system_return_val == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
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
    // initialize pid and get return value of fork
    pid_t pid;
    pid = fork();
    
    // if pid = -1, error
    if (pid == -1)
    {
        perror("Error happened when forking");
        return false;
    }
    else if (pid == 0)
    {
        // child process
        int execv_return_val;
        printf("Child process: %s\n", command[0]);
        execv_return_val = execv(command[0], command);
        
        // if execvReturVal gets value then there was an error
        printf("Error executing execv with error %d\n", execv_return_val);
        exit(execv_return_val);
    }
    else
    {
        // parent process
        int status, exit_status;
        pid_t child_pid;
        
        // wait for child process to complete
        child_pid = waitpid(pid, &status, 0);
        
        // check status of child process
        exit_status = WIFEXITED(status);
        if (exit_status != 0)
        {
            int error_status = WEXITSTATUS(status);
            if (error_status)
            {
                printf("Error with child pid: %d\n", child_pid);
                printf("Child exited with status: %d\n", exit_status);
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

    int redirect_fd;
    
    // open file
    redirect_fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    //if error opening file
    if (redirect_fd < 0)
    {
        perror("Error opening file\n");
        return false;
    }
    
    pid_t pid;
    switch (pid = fork())
    {
        case -1:
            // error
            perror("Forking error when forking\n");
            return false;
        case 0:
            // child process
            if (dup2(redirect_fd, 1) < 0)
            {
                perror("dup2 error redirect file\n");
                return false;
            }
            close(redirect_fd);
            
            //run child process
            int execv_return_value;
            execv_return_value = execv(command[0], command);
            // if return value
            printf("Eroror executing execv_return_value: %d\n", execv_return_value);
            exit(execv_return_value);
        default:
            close(redirect_fd);
            // parent process
            int status, exit_status;
            pid_t child_pid;
            
            // wait for child process to complete
            child_pid = waitpid(pid, &status, 0);
            
            // check status of child process
            exit_status = WIFEXITED(status);
            if (exit_status != 0)
            {
                int error_status = WEXITSTATUS(status);
                if (error_status)
                {
                    printf("Error with child pid: %d\n", child_pid);
                    printf("Child exited with status: %d\n", exit_status);
                    return false;
                }
            }
    }

    va_end(args);

    return true;
}
