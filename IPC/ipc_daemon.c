#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <sys/select.h>

#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"
#define FIFO3 "/tmp/fifo3"
#define CMD "COMPARE\n"
#define LOG_FILE "daemon_log.log"
#define TIMEOUT 15


volatile sig_atomic_t child_count = 0;
volatile sig_atomic_t total_children = 2;
volatile sig_atomic_t running = 1;
int fd_log;

/* Handler for the timeout signal */
void timeout_handler() {
    write(fd_log, "Timeout occurred, terminating child processes\n", 44);
    // Terminate all child processes
    kill(0, SIGTERM); // Send SIGTERM to all processes in the same process group
    running = 0; // Stop the main loop
}

/* Handler for SIGCHLD signal */
void sigchld_handler() {
    pid_t pid;
    int status;
    char msg[100];
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            snprintf(msg, sizeof(msg), "Child process %d terminated with exit status %d\n", pid, WEXITSTATUS(status));
            write(fd_log, msg, strlen(msg));
        } else if (WIFSIGNALED(status)) {
            snprintf(msg, sizeof(msg), "Child process %d terminated by signal %d\n", pid, WTERMSIG(status));
            write(fd_log, msg, strlen(msg));
        } else {
            snprintf(msg, sizeof(msg), "Child process %d terminated with unknown status\n", pid);
            write(fd_log, msg, strlen(msg));
        }
        child_count++;
    }
    
    // Check if all child processes have terminated
    if (child_count >= total_children) {
        running = 0;
    }
}

/* Signal handler for SIGUSR1, SIGHUP, and SIGTERM signals */
void signal_handler(int sig) {
    switch(sig) {
        case SIGUSR1:
            write(fd_log, "Received SIGUSR1 signal\n", 25);
            break;
        case SIGHUP:
            write(fd_log, "Received SIGHUP signal, reloading configuration\n", 49);
            break;
        case SIGTERM:
            write(fd_log, "Received SIGTERM signal, terminating\n", 38);
            running = 0;
            break;
    }
}

/* Cleanup function to remove FIFOs and close log file */
void clean_data() {
    unlink(FIFO1);
    unlink(FIFO2);
    unlink(FIFO3);
    if (fd_log > 0) {
        char msg[100];
        snprintf(msg, sizeof(msg), "Daemon process %d cleaning up and exiting with exit status %d\n", getpid(), 0);
        write(fd_log, msg, strlen(msg));
        close(fd_log);
    }
}

/* Creating daemon process with two fork method*/
void create_daemon() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGUSR1, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    fd_log = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_log < 0) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }
    
    chdir("/");
    umask(0);
    dup2(fd_log, STDOUT_FILENO);
    dup2(fd_log, STDERR_FILENO);
        
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Log start time and PID
    time_t now = time(NULL);
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Daemon started at %s with PID: %d\n", ctime(&now), getpid());
    write(fd_log, log_msg, strlen(log_msg));
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Integer> <Integer>\n", argv[0]);
        return 1;
    }
    // Converting parent process to daemon
    create_daemon();


    int num1 = atoi(argv[1]);
    int num2 = atoi(argv[2]);
    int result = 0;
    int fd1, fd2, fd3;

    if (mkfifo(FIFO1, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO1");
        return 1;
    }
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO2");
        unlink(FIFO1);
        return 1;
    }
    if (mkfifo(FIFO3, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO3");
        unlink(FIFO1);
        unlink(FIFO2);
        return 1;
    }

    signal(SIGALRM, timeout_handler);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child 1 - Reads from FIFO1, compares, writes to FIFO2
        char msg[100];
        snprintf(msg, sizeof(msg), "Child1 started with PID: %d\n", getpid());
        write(fd_log, msg, strlen(msg));
        
        fd1 = open(FIFO1, O_RDONLY);
        sleep(10);
        if (fd1 == -1) {
            write(fd_log, "Child1: Error opening FIFO1\n", 27);
            exit(EXIT_FAILURE);
        }

        alarm(TIMEOUT);

        if (read(fd1, &num1, sizeof(num1)) != sizeof(num1) || 
            read(fd1, &num2, sizeof(num2)) != sizeof(num2)) {
            write(fd_log, "Child1: Error reading numbers\n", 29);
            close(fd1);
            exit(EXIT_FAILURE);
        }
        alarm(0);
        char msg2[100];
        snprintf(msg2, sizeof(msg2), "Child1 read numbers: %d, %d\n", num1, num2);
        write(fd_log, msg2, strlen(msg2));
        close(fd1);

        result = (num1 > num2) ? num1 : num2;
        
        fd3 = open(FIFO3, O_WRONLY);
        if (fd3 == -1) {
            write(fd_log, "Child1: Error opening FIFO3\n", 27);
            exit(EXIT_FAILURE);
        }
        write(fd3, &result, sizeof(result));
        char msg3[100];
        snprintf(msg3, sizeof(msg3), "Child1 wrote result: %d\n", result);
        write(fd_log, msg3, strlen(msg3));
        close(fd3);
        exit(EXIT_SUCCESS);
    } 
    else if (pid1 > 0) {
        pid_t pid2 = fork();
        if (pid2 == 0) {
            // Child 2 - Reads command from FIFO2, then result
            char msg[100];
            snprintf(msg, sizeof(msg), "Child2 started with PID: %d\n", getpid());
            write(fd_log, msg, strlen(msg));
            
            
            fd2 = open(FIFO2, O_RDONLY);
            sleep(10);
            if (fd2 == -1) {
                write(fd_log, "Child2: Error opening FIFO2\n", 27);
                exit(EXIT_FAILURE);
            }
            alarm(TIMEOUT);

            // First read the command
            char cmd[10];
            if (read(fd2, cmd, sizeof(CMD)) != sizeof(CMD)) {
                write(fd_log, "Child2: Error reading command\n", 29);
                close(fd2);
                exit(EXIT_FAILURE);
            }
            alarm(0);
            char msg2[100];
            snprintf(msg2, sizeof(msg2), "Child2 read command: %s", cmd);
            write(fd_log, msg2, strlen(msg2));
            close(fd2);

            fd3 = open(FIFO3, O_RDONLY);
            if (fd3 == -1) {
                write(fd_log, "Child2: Error opening FIFO3\n", 27);
                exit(EXIT_FAILURE);
            }
            alarm(TIMEOUT);

            // Then read the result (written by child1)
            if (read(fd3, &result, sizeof(result)) != sizeof(result)) {
                write(fd_log, "Child2: Error reading result\n", 29);
                close(fd3);
                exit(EXIT_FAILURE);
            }
            alarm(0);
            char msg3[100];
            snprintf(msg3, sizeof(msg3), "Child2 read result: %d\n", result);
            write(fd_log, msg3, strlen(msg3));
            close(fd3);
            
            char msg4[100];
            snprintf(msg4, sizeof(msg4), "Child2: The result is: %d\n", result);
            write(fd_log, msg4, strlen(msg4));

            exit(EXIT_SUCCESS);
        }
        else if (pid2 > 0) {
            char msg[100];
            snprintf(msg, sizeof(msg), "Daemon started with PID: %d\n", getpid());
            write(fd_log, msg, strlen(msg));
            
            sleep(1); // Let children start
            
            // Write numbers to FIFO1
            fd1 = open(FIFO1, O_WRONLY);
            if (fd1 == -1) {
                write(fd_log, "Parent: Error opening FIFO1\n", 27);
                kill(pid1, SIGTERM);
                kill(pid2, SIGTERM);
                clean_data();
                return 1;
            }
            write(fd1, &num1, sizeof(num1));
            write(fd1, &num2, sizeof(num2));
            char msg2[100];
            snprintf(msg2, sizeof(msg2), "Parent wrote numbers: %d, %d\n", num1, num2);
            write(fd_log, msg2, strlen(msg2));
            close(fd1);

            alarm(TIMEOUT); // Set timeout for child processes

            
            // Write command to FIFO2
            fd2 = open(FIFO2, O_WRONLY);
            if (fd2 == -1) {
                write(fd_log, "Parent: Error opening FIFO2\n", 27);
                kill(pid1, SIGTERM);
                kill(pid2, SIGTERM);
                clean_data();
            }
            write(fd2, CMD, strlen(CMD)+1);
            char msg3[100];
            snprintf(msg3, sizeof(msg3), "Parent wrote command: %s", CMD);
            write(fd_log, msg3, strlen(msg3));
            close(fd2);
            
            alarm(0); // Reset timeout
            
            signal(SIGCHLD, sigchld_handler);
            
            while (running) {
                write(fd_log, "proceeding\n", 11);
                sleep(2);
                if (child_count >= total_children) {
                    running = 0;
                }
            }
            // Wait for children to finish
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
            

            clean_data();
            return 0;
        }
        else {
            write(fd_log, "Error forking child 2\n", 22);
            kill(pid1, SIGTERM);
            wait(NULL);
            clean_data();
            return 1;
        }
    }
    else {
        write(fd_log, "Error forking child 1\n", 22);
        clean_data();
        return 1;
    }
}