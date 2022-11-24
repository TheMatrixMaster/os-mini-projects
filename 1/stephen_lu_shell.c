/* README
Name        |   Stephen Lu
Student ID  |   260343328

This C program runs a basic shell with redirection and piping support
One particularity with this shell which was not specified by the assignment instructions is that we decided 
to apply a limit on the number of background processes that are concurrently running to prevent memory leaks.
We have currently set this limit to 5, but it can be easily modified through the `MAX_BG_PROCESSES` global var.

The following list of built-in commands are executed in the foreground with no forking:
    $ exit
    $ echo
    $ pwd
    $ cd <valid_directory>
    $ jobs
    $ fg <integer>

External commands are executed in a child process using fork() and execvp(). Here are some examples:
    $ cat
    $ ls -ltra
    $ rm <filename>
    $ ps

Invalid external commands will raise an error on the execvp() system call and set errno to the appropriate error code.

The default behaviour for all commands is for the parent to wait synchronously for the command to finish executing. 
However, the shell supports background processing for external commands. To use this feature, please append 
the `&` character to the end of your command. Here are some examples:
    $ sleep 10 &
    $ cat <filename> &
    $ rm -rf <dir> &

As mentioned previously, redirection and piping are also supported on external commands. Here is the correct syntax to use
these features. Please pay attention to the spaces surrounding the `|` and `>` characters:
    // redirection
    $ ls -ltra > file.txt
    $ cat <filename> > new_file.txt

    // piping
    $ cat <filename> | wc -l
    $ cat <filename> | grep "some regular expression"

Finally, our shell also handles some basic signals:
    - CTRL-Z signal is ignored by our shell
    - CTRL-C signal will kill any child process currently running in the foreground
    - CTRL-D signal will kill the main parent process and may leave zombie/orphan child processes
*/


#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

/* maximum number of args in a command */
#define LENGTH 20 
#define MAX_BG_PROCESSES 5

/* define bgProcess struct to store background process information */
typedef struct 
{
    pid_t id;
    char* cmd;
} bgProcess;

/* we use a global variable to store pid of active process in foreground */
pid_t active_pid;

/* we use global variables to track background processes */
bgProcess *processes;
int numBgProcesses = 0;

char is_number(char *text)
{
    int j;
    j = strlen(text);
    while(j--)
    {
        if(text[j] > 47 && text[j] < 58)
            continue;

        return 0;
    }
    return 1;
}

void handle_sigint(int signal) 
{
    if (active_pid != -1) {
        printf("Killing %d\n", active_pid);
        kill(active_pid, SIGKILL);
    }
    return;
}

int remove_process(int pid) 
{
    int found = 0;
    for (int i=0; i < numBgProcesses; i++) {
        if (found) {
            // if we've found the process, we can shift down all further processes
            memcpy(&processes[i-1], &processes[i], sizeof(processes[i]));
        } else if (processes[i].id == pid) {
            // we've found the process, so we set found to pid
            found = pid;
        }
    }

    return found;
}

void handle_sigchld(int signal) 
{
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        if (remove_process(pid)) {
            // if found, we decrement the number of background processes by 1
            numBgProcesses -= 1;
        }
    }
}

/* Parses the user command in char *args[]. 
   Returns the number of entries in the array */

int getcmd(char *prompt, char *args[], char *raw_cmd, int *background, int *piping, int *redirection) 
{

    int length, flag, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length == 0) return 0;          // check for invalid command
    else if (length == -1) exit(0);     // check for CTRL + D => exit main shell

    /* check if background is specified */
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else {
        *background = 0;
    }
    
    // Clear args
    memset(args, '\0', LENGTH);
    strcpy(raw_cmd, line);

    // Splitting the command and putting the tokens inside args[]
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) {
                token[j] = '\0'; 
            } else if (token[j] == 124) {
                token[j] = '\0';
                *piping = i;
            } else if (token[j] == 62) {
                token[j] = '\0';
                *redirection = i;
            }
        }
        if (strlen(token) > 0) {
            args[i++] = token;
        }
    }

    // make sure args ends with NULL pointer for execvp
    args[i] = NULL;
    
    return i;
}


int main(void) { 
    char* args[LENGTH];             /* string array to store user arguments */
    int redirection;                /* flag for output redirection */
    int piping;                     /* flag for piping */
    int bg;                         /* flag for running processes in the background */
    int cnt;                        /* count of the arguments in the command */
    char raw_cmd[_SC_ARG_MAX];      /* string to store raw user command */

    // Check for signals
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        printf("ERROR: could not bind signal handler for SIGINT\n");
        exit(EXIT_FAILURE);
    }
    
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        printf("ERROR: could not bind signal handler for SIGTSTP\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR) {
        printf("ERROR: could not bind signal handler for SIGCHLD\n");
        exit(EXIT_FAILURE);
    }

    /* allocate memory for bgProcess array */
    processes = malloc(MAX_BG_PROCESSES * sizeof(bgProcess));
    for (int p=0; p < MAX_BG_PROCESSES; p++) {
        processes[p].id = -1;
        processes[p].cmd = calloc(_SC_ARG_MAX, sizeof(char));
    }

    while(1) {
        // reset flags 
        bg = 0;
        redirection = 0;
        piping = 0;
        active_pid = -1;

        if ((cnt = getcmd("$ ", args, raw_cmd, &bg, &piping, &redirection)) <= 0) {
            // no args were inputted to terminal, so we ask for input again.
            continue;
        }

        // store cmd in string for ease of use
        char* cmd = args[0];

        // built-in commands
        if (strcmp(cmd, "echo") == 0)
        {
            printf("%s", raw_cmd + 5);
            continue;
        } 
        else if (strcmp(cmd, "pwd") == 0) 
        {
            char* buffer;
            if( (buffer=getcwd(NULL, 0)) == NULL) {
                printf("Failed to get current directory\n");
                exit(1);
            } else {
                printf("%s\n", buffer);
                free(buffer);
            }
            continue;
        } 
        else if (strcmp(cmd, "jobs") == 0) 
        {
            if (numBgProcesses > 0) {
                for (int i=0; i<numBgProcesses; i++) {
                    printf("%d: %s", processes[i].id, processes[i].cmd);
                }
            } else {
                printf("There are no running background jobs\n");
            }
            continue;
        }
        // on exit cmd, we terminate background processes and exit main shell
        else if (strcmp(cmd, "exit") == 0) 
        {
            for (int i=0; i<numBgProcesses; i++) {
                kill(processes[i].id, SIGKILL);
            }
            exit(EXIT_SUCCESS);
        }
        // on fg [pid], we bring pid from background to foreground
        else if (strcmp(cmd, "fg") == 0)
        {
            if (args[1] && is_number(args[1])) {
                int bg_index = atoi(args[1]);

                if (bg_index < numBgProcesses) {
                    int bg_pid = processes[bg_index].id;
                    if (remove_process(bg_pid)) {
                        numBgProcesses -= 1;
                        active_pid = bg_pid;
                        waitpid(bg_pid, NULL, 0);
                        active_pid = -1;
                    } else {
                        printf("Something went wrong when killing pid: %d\n", bg_pid);
                    }
                } else {
                    printf("There are only %d background processes but you provided an argument %d\n", numBgProcesses, bg_index);
                }
            } else {
                printf("The argument you provided is not a valid number\n");
            }
            continue;
        }
        // chdir to provided directory
        else if (strcmp(cmd, "cd") == 0) 
        {
            if (chdir(args[1]) != 0) {
                printf("An invalid directory was provided\n");
            }
            continue;
        }

        pid_t pid = fork();

        if (pid == -1) {
            printf("ERROR: fork failed\n");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {

            if (redirection) 
            {
                int fd;
                close(STDOUT_FILENO);

                if (fd = open(args[redirection], O_WRONLY | O_CREAT, 0644) < 0) {
                    printf("Output redirection failed. Could not open file\n");
                    exit(EXIT_FAILURE);
                }
                close(fd);

                for (int l = 0; l < cnt; l++) {
                    if (l >= redirection) args[l] = NULL;
                }

                execvp(args[0], args);
                printf("execvp exited with error code %d\n", errno);
                exit(errno);

            } 
            else if (piping) 
            {
                char* cmd1[LENGTH];
                char* cmd2[LENGTH];
                memset(cmd1, '\0', LENGTH);
                memset(cmd2, '\0', LENGTH);

                for (int l = 0; l <= cnt; l++) {
                    if (l < piping) {
                        cmd1[l] = args[l];
                    } else if (l < cnt) {
                        cmd1[l] = NULL;
                        cmd2[l - piping] = args[l];
                    } else {
                        cmd1[l] = NULL;
                        cmd2[l - piping] = NULL;
                    }
                }

                int forward[2];
                if (pipe(forward) < 0) {
                    printf("pipe() exited with a non-zero code\n");
                    exit(EXIT_FAILURE);
                };

                pid_t pipe_pid = fork();

                if (pipe_pid < 0) {
                    printf("fork() failed during piping\n");
                    exit(EXIT_FAILURE);
                } else if (pipe_pid == 0) {
                    // child process executes first command
                    close(forward[0]);
                    close(STDOUT_FILENO);
                    dup(forward[1]);

                    execvp(cmd1[0], cmd1);
                    printf("execvp exited with error code %d\n", errno);
                    exit(errno);
                } else {
                    // parent process executes second command
                    close(forward[1]);
                    close(STDIN_FILENO);
                    dup(forward[0]);

                    execvp(cmd2[0], cmd2);
                    printf("execvp exited with error code %d\n", errno);
                    exit(errno);
                }

            } 
            else 
            {
                execvp(args[0], args);
                printf("execvp failed with %d\n", errno);
                exit(errno);
            }

        } else {
            if (bg == 0) {
                /* since child is running in foreground, we update active_pid */
                active_pid = pid;

                /* child process is not in the bg, so wait for it to exit */
                waitpid(pid, NULL, 0);

                /* child process has finished executing, so we update active_pid again */
                active_pid = -1;

            } else {
                // user wants to run child process in background so we add new bgProcess
                if (numBgProcesses >= MAX_BG_PROCESSES) {
                    printf("You have reached the maximum number of active background processes\n");
                } else {
                    processes[numBgProcesses].id = pid;
                    strcpy(processes[numBgProcesses].cmd, raw_cmd);
                    numBgProcesses += 1;
                }
            }
        }
    }
}