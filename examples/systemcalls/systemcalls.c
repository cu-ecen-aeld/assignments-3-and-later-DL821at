#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    if (!cmd) return false; // Check for null command

    int status = system(cmd);
    return (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0);
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
    int status;  // Variable to store the status of the child process

    va_list args;
    va_start(args, count);
    char *command[count + 1];  // Create an array to store the command and arguments
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);  // Populate the array with arguments
    }
    command[count] = NULL;  // Terminate the array with NULL for execv
    va_end(args);

    pid = fork();  // Create a child process
    if (pid == -1) {
        return false;  // Fork failed
    }

    if (pid == 0) {
        // Child process: execute the command
        execv(command[0], command);
        exit(EXIT_FAILURE);  // If execv fails, exit with failure status
    } 
    else {
        // Parent process: wait for the child to terminate
        if (waitpid(pid, &status, 0) == -1) {
            return false;  // Waiting failed
        }
        // Check if the child process exited normally and the exit status was 0
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
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
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    pid_t pid = fork();
    if (pid == -1) {
        va_end(args);
        return false;  // Fork failed
    }

    if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            exit(EXIT_FAILURE);  // Fail if unable to open the output file
        }
        dup2(fd, STDOUT_FILENO);  // Redirect stdout to the output file
        close(fd);

        execv(command[0], command);
        exit(EXIT_FAILURE);  // If execv fails, exit with failure status
    }

    int status;
    waitpid(pid, &status, 0);
    va_end(args);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;

}
