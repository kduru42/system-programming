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

#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"
#define CMD "COMPARE\n"
#define LOG_FILE "/var/log/daemon_log.log"

volatile sig_atomic_t child_count = 0;
volatile sig_atomic_t total_children = 2;
volatile sig_atomic_t running = 1;

void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        syslog(LOG_INFO, "Child process %d terminated", pid);
        child_count += 2;
    }
    
    if (child_count >= total_children) {
        running = 0;
    }
}

void signal_handler(int sig) {
    switch(sig) {
        case SIGUSR1:
            syslog(LOG_INFO, "Received SIGUSR1 signal");
            break;
        case SIGHUP:
            syslog(LOG_INFO, "Received SIGHUP signal");
            break;
        case SIGTERM:
            syslog(LOG_INFO, "Received SIGTERM signal, shutting down");
            running = 0;
            break;
    }
}

void create_daemon() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    signal(SIGCHLD, sigchld_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    
    chdir("/");
    umask(0);
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp) {
        dup2(fileno(log_fp), STDOUT_FILENO);
        dup2(fileno(log_fp), STDERR_FILENO);
        fclose(log_fp);
    }
    
    openlog("DaemonProcess", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon started, PID: %d", getpid());
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Integer> <Integer>\n", argv[0]);
        return 1;
    }

    int num1 = atoi(argv[1]);
    int num2 = atoi(argv[2]);
    int result = 0;
    int fd1, fd2;

    // Create FIFOs
    if (mkfifo(FIFO1, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO1");
        return 1;
    }
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO2");
        unlink(FIFO1);
        return 1;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child 1 - Reads from FIFO1, compares, writes to FIFO2
        sleep(10);
        
        fd1 = open(FIFO1, O_RDONLY);
        if (fd1 == -1) {
            syslog(LOG_ERR, "Child1: Error opening FIFO1");
            exit(1);
        }

        if (read(fd1, &num1, sizeof(num1)) != sizeof(num1) || 
            read(fd1, &num2, sizeof(num2)) != sizeof(num2)) {
            syslog(LOG_ERR, "Child1: Error reading numbers");
            close(fd1);
            exit(1);
        }
        close(fd1);

        result = (num1 > num2) ? num1 : num2;
        
        fd2 = open(FIFO2, O_WRONLY);
        if (fd2 == -1) {
            syslog(LOG_ERR, "Child1: Error opening FIFO2");
            exit(1);
        }
        write(fd2, &result, sizeof(result));
        close(fd2);
        exit(0);
    } 
    else if (pid1 > 0) {
        pid_t pid2 = fork();
        if (pid2 == 0) {
            // Child 2 - Reads command from FIFO2, then result
            sleep(10);
            
            fd2 = open(FIFO2, O_RDONLY);
            if (fd2 == -1) {
                syslog(LOG_ERR, "Child2: Error opening FIFO2");
                exit(1);
            }

            // First read the command
            char cmd[10];
            if (read(fd2, cmd, sizeof(CMD)) != sizeof(CMD)) {
                syslog(LOG_ERR, "Child2: Error reading command");
                close(fd2);
                exit(1);
            }

            // Then read the result (written by child1)
            if (read(fd2, &result, sizeof(result)) != sizeof(result)) {
                syslog(LOG_ERR, "Child2: Error reading result");
                close(fd2);
                exit(1);
            }
            close(fd2);
            
            // This will go to syslog and log file
            printf("The greater number is: %d\n", result);
            exit(0);
        }
        else if (pid2 > 0) {
            // Convert to daemon
            create_daemon();
            
            sleep(1); // Let children start
            
            // Write numbers to FIFO1
            fd1 = open(FIFO1, O_WRONLY);
            if (fd1 == -1) {
                syslog(LOG_ERR, "Parent: Error opening FIFO1");
                kill(pid1, SIGTERM);
                kill(pid2, SIGTERM);
                goto cleanup;
            }
            write(fd1, &num1, sizeof(num1));
            write(fd1, &num2, sizeof(num2));
            close(fd1);
            
            // Write command to FIFO2
            fd2 = open(FIFO2, O_WRONLY);
            if (fd2 == -1) {
                syslog(LOG_ERR, "Parent: Error opening FIFO2");
                kill(pid1, SIGTERM);
                kill(pid2, SIGTERM);
                goto cleanup;
            }
            write(fd2, CMD, strlen(CMD)+1);
            close(fd2);
            
            // Main loop
            while (running) {
                syslog(LOG_INFO, "proceeding");
                sleep(2);
                
                if (child_count >= total_children) {
                    running = 0;
                }
            }
            
            cleanup:
            // Wait for any remaining children
            while (wait(NULL) > 0);
            
            // Cleanup
            unlink(FIFO1);
            unlink(FIFO2);
            syslog(LOG_INFO, "Daemon shutting down");
            closelog();
            return 0;
        }
        else {
            syslog(LOG_ERR, "Error forking child 2");
            kill(pid1, SIGTERM);
            wait(NULL);
            goto cleanup;
        }
    }
    else {
        syslog(LOG_ERR, "Error forking child 1");
        goto cleanup;
    }
}