#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <sys/time.h>

#define CONFIG_FILE "/etc/daemon_config.conf"
volatile sig_atomic_t running = 1; // Flag to indicate if the daemon is running

char LOG_FILE[100] = "/var/log/daemon_log.txt";
int timeout = 20;
char FIFO_NAME[100] = "/tmp/fifo1";
char FIFO_NAME2[100] = "/tmp/fifo2";

int child_counter = 0; // Number of child processes created
int total_children = 2; // Total number of child processes to create

void reload_config() {
    FILE *config_file = fopen(CONFIG_FILE, "r");
    if (config_file == NULL) {
        syslog(LOG_ERR, "Error opening config file: %s", strerror(errno));
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), config_file)) {
        if (strncmp(line, "LOG_FILE=", 9) == 0) {
            sscanf(line + 9, "%s", LOG_FILE);
        } else if (strncmp(line, "TIMEOUT=", 8) == 0) {
            sscanf(line + 8, "%d", &timeout);
        } else if (strncmp(line, "FIFO_NAME=", 10) == 0) {
            sscanf(line + 10, "%s", FIFO_NAME);
        } else if (strncmp(line, "FIFO_NAME2=", 11) == 0) {
            sscanf(line + 11, "%s", FIFO_NAME2);
        }
    }
    fclose(config_file);
    syslog(LOG_INFO, "Configuration reloaded: LOG_FILE=%s, TIMEOUT=%d, FIFO_NAME=%s, FIFO_NAME2=%s",
        LOG_FILE, timeout, FIFO_NAME, FIFO_NAME2);
}   

void sigchld_handler(int signo)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        char msg[100];
        char pid_str[20];
        strcat(msg, "Child process ");
        sprintf(pid_str, "%d", pid);
        strcat(msg, pid_str);
        strcat(msg, " terminated\n");
        write (1, msg, strlen(msg));
        child_counter += 2;
    }
}

// **Signal Handler**
void handle_signal(int signo) {
    if (signo == SIGTERM) {
        syslog(LOG_INFO, "Daemon received SIGTERM. Exiting gracefully.");
        running = 0;  // Set the running flag to 0 to exit the loop in main
        closelog();  // Close the syslog connection
    } else if (signo == SIGHUP) {
        syslog(LOG_INFO, "Daemon received SIGHUP. Reloading configuration.");
        reload_config();
    }
}

void monitor_children() {
    int status;
    pid_t child_pid;
    time_t start_time = time(NULL);

    while (child_counter > 0) {
        child_pid = waitpid(-1, &status, WNOHANG);

        if (child_pid > 0) {  // A child process terminated
            syslog(LOG_INFO, "Child process %d terminated", child_pid);
            child_counter--;
        } else if (child_pid == 0) {  // No child has exited yet
            sleep(1);  // Avoid busy-waiting
        } else {  // No child left (-1 returned)
            break;
        }

        // Check timeout
        if (time(NULL) - start_time > timeout) {
            syslog(LOG_WARNING, "Timeout reached. Checking for inactive child processes.");
            
            // Kill remaining children
            while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
                syslog(LOG_WARNING, "Killing child process %d due to timeout", child_pid);
                kill(child_pid, SIGTERM);
                child_counter--;
            }
            break;
        }
    }
}

void create_deamon()
{
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);  // Parent exits

    if (setsid() < 0) exit(EXIT_FAILURE);  // Create a new session

    signal(SIGCHLD, sigchld_handler);
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);  // Parent exits
    
    chdir("/");
    umask(0);
    
    // Redirect standard output and error to the log file
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp) {
        dup2(fileno(log_fp), STDOUT_FILENO);
        dup2(fileno(log_fp), STDERR_FILENO);
        fclose(log_fp);
    }
    
    openlog("DaemonProcess", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon started, PID: %d", getpid());
    signal(SIGTERM, handle_signal);  // Handle termination signal
    signal(SIGHUP, handle_signal);  // Handle hangup signal
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        write(2, "Usage: ./main <Integer> <Integer>\n", 35);
        return 1;
    }
    int num1 = atoi(argv[1]);
    int num2 = atoi(argv[2]);
    int result = 0;
    
    char buffer[100];
    char buffer2[100];
    int fd;
    int fd2;
    create_deamon();

    // Create a daemon process
    // Create the FIFO (named pipe)
    if (mkfifo(FIFO_NAME, 0666) == -1) {
        syslog(LOG_ERR, "Error creating FIFO: %s", strerror(errno));
        return 1;
    }
    
    if (mkfifo(FIFO_NAME2, 0666) == -1) {
        syslog(LOG_ERR, "Error creating FIFO2: %s", strerror(errno));
        return 1;
    }
    
    
    fd = open(FIFO_NAME, O_WRONLY);
    if (fd == -1) {
        syslog(LOG_ERR, "Error opening FIFO: %s", strerror(errno));
        return 1;
    }
    write (fd, &num1, sizeof(num1));
    write (fd, &num2, sizeof(num2));
    close(fd);
    
    fd2 = open(FIFO_NAME2, O_WRONLY);
    if (fd2 == -1) {
        syslog(LOG_ERR, "Error opening FIFO2: %s", strerror(errno));
        return 1;
    }
    write (fd2, "COMPARE\n", 8);
    close(fd2);
    
    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child process 1
        syslog(LOG_INFO, "Child process 1 started, PID: %d", getpid());
        sleep(10);
        fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK);
        if (fd == -1) {
            syslog(LOG_ERR, "Error opening FIFO: %s", strerror(errno));
            return 1;
        }
        read(fd, &num1, sizeof(num1));
        read(fd, &num2, sizeof(num2));
        close(fd);

        fd2 = open(FIFO_NAME2, O_RDONLY | O_NONBLOCK);
        if (fd2 == -1) {
            syslog(LOG_ERR, "Error opening FIFO2: %s", strerror(errno));
            return 1;
        }
        read(fd2, buffer, sizeof(buffer));
        close(fd2);
        if (strcmp(buffer, "COMPARE\n") != 0) {
            syslog(LOG_ERR, "Error reading from FIFO2: %s", strerror(errno));
            return 1;
        }
        else
        {
            if (num1 > num2)
                result = num1;
            else
                result = num2;
        }
        fd = open(FIFO_NAME2, O_WRONLY);
        if (fd == -1) {
            syslog(LOG_ERR, "Error opening FIFO2 for writing: %s", strerror(errno));
            return 1;
        }
        write(fd, &result, sizeof(result));
        close(fd);
        syslog(LOG_INFO, "Result written to FIFO2: %d", result);
        syslog(LOG_INFO, "Child process 1 finished, PID: %d", getpid());
        exit(EXIT_SUCCESS);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        syslog(LOG_INFO, "Child process 2 started, PID: %d", getpid());
        // Second child process: Read and print the result
        sleep(10);
        fd2 = open(FIFO_NAME2, O_RDONLY | O_NONBLOCK);
        if (fd2 == -1) {
            syslog(LOG_ERR, "Error opening FIFO2 for reading: %s", strerror(errno));
            exit(1);
        }
        read(fd2, &result, sizeof(result));
        close(fd2);

        char msg[100];
        strcat(msg, "The greater number is: ");
        char result_str[20];
        sprintf(result_str, "%d", result);
        strcat(msg, result_str);
        strcat(msg, "\n");
        write(1, msg, strlen(msg));

        syslog(LOG_INFO, "Result read from FIFO2: %d", result);
        syslog(LOG_INFO, "Child process 2 finished, PID: %d", getpid());
        exit(EXIT_SUCCESS);
    }

    while ((child_counter < total_children) && running)
    {
        write(1, "Proceeding...\n", 15);
        monitor_children();
        sleep(2);
    }

    closelog();
    unlink(FIFO_NAME);
    unlink(FIFO_NAME2);
    return 0;
}