#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
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

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int ret = system(cmd);

    if (ret == 0) {
        return true;
    } else {
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
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    if (command[0][0] != '/') {
        printf("The command given to do_exec() needs to be an absolute path.\n");
        return false;
    }

    fflush(stdout);
    int pid = fork();
    if (pid == -1) {
        perror("fork() in do_exec() failed");
        return false;
    } else if (pid == 0) {
        int ret = execv(command[0], command);
        if (ret == -1) {
            perror("execv() in do_exec() failed");
            return false;
        }
    } else {
        int wstatus;
        int ret = waitpid(pid, &wstatus, 0);
        if (ret == -1) {
            perror("wait() in do_exec() failed");
            return false;
        } else if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
            printf("%s exited with non-zero exit status %d\n", command[0], WEXITSTATUS(wstatus));
            return false;
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
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    if (command[0][0] != '/') {
        printf("The command given to do_exec() needs to be an absolute path.\n");
        return false;
    }

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) {
        perror("open() in do_exec_redirect() failed");
        return false;
    }

    fflush(stdout);
    int pid = fork();
    if (pid == -1) {
        perror("fork() in do_exec() failed");
        return false;
    } else if (pid == 0) {
        if (dup2(fd, 1) < 0) {
            perror("dup2() in do_exec_redirect() failed");
            return false;
        }
        close(fd);
        int ret = execv(command[0], command);
        if (ret == -1) {
            perror("execv() in do_exec() failed");
            return false;
        }
    } else {
        close(fd);
        int wstatus;
        int ret = wait(&wstatus);
        if (ret == -1) {
            perror("wait() in do_exec() failed");
            return false;
        } else if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
            printf("%s exited with non-zero exit status %d\n", command[0], WEXITSTATUS(wstatus));
            return false;
        }
    }

    va_end(args);

    return true;
}
