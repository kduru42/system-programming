#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <sys/file.h>

#define MAX_ARGS 10

void split(char *input, char *args[]) {
    int i = 0;
    char *token = strtok(input, " "); // Split by space

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL; // Null-terminate the argument list
}

int isFileLocked(const char *fileName)
{
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file");
        return -1;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;  // Check for write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;         // Check entire file

    if (fcntl(fd, F_GETLK, &lock) == -1)
    {
        perror("fcntl error");
        close(fd);
        return -1;
    }

    close(fd);
    return (lock.l_type == F_UNLCK) ? 0 : 1; // 0 = not locked, 1 = locked
}

char *getTimestamp() {
    time_t now;
    struct tm *timeinfo;
    char *timestamp = malloc(20 * sizeof(char));

    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, 20, "[%Y-%m-%d %H:%M:%S]", timeinfo);

    return timestamp;
}

void createDir(char *dirName)
{
    struct stat st = {0};
    dirName[strlen(dirName) - 1] = '\0'; // Remove newline character

    if (stat(dirName, &st) == -1)
    {
        if (mkdir(dirName, 0700) == 0)
            write(1, "Directory created successfully\n\n", 32);
        else
            perror("Error creating directory");
    }
    else
    {
        char msg[100];
        strcat(msg, "Error: Directory ");
        strcat(msg, dirName);
        strcat(msg, " already exists\n\n");
        write(1, msg, strlen(msg));
    }
}


void creteFile(char *fileName)
{
    struct stat st = {0};
    fileName[strlen(fileName) - 1] = '\0'; // Remove newline character

    if (stat(fileName, &st) == -1)
    {
        int fd = open(fileName, O_CREAT | O_WRONLY, 0700);
        if (fd != -1)
        {
            write(1, "File created successfully\n\n", 27);
            write(fd, getTimestamp(), 19);          
            close(fd);
        }
        else
        {
            perror("Error creating file");
        }
    }
    else
    {
        char msg[100];
        strcat(msg, "Error: File ");
        strcat(msg, fileName);
        strcat(msg, " already exists\n\n");
        write(1, msg, strlen(msg));
    }
}

void listDir (char *dirName)
{
    struct dirent *de;
    struct stat st = {0};
    dirName[strlen(dirName) - 1] = '\0'; // Remove newline character
    if (stat(dirName, &st) == -1)
    {   
        char msg[100];
        strcat(msg, "Error: Directory ");
        strcat(msg, dirName);
        strcat(msg, " nout found.\n");
        write(1, msg, strlen(msg));
        return;
    }

    DIR *dr = opendir(dirName);
    if (dr == NULL)
    {
        write(1, "Could not open directory\n", 25);
        return;
    }
    while ((de = readdir(dr)) != NULL)
    {
        write(1, de->d_name, strlen(de->d_name));
        write(1, "\n", 1);
    }
    closedir(dr);
}

void listFileByExtension(char *dirName, char *extension)
{
    struct dirent *de;
    struct stat st = {0};
    extension[strlen(extension) - 1] = '\0'; // Remove newline character
    if (stat(dirName, &st) == -1)
    {   
        char msg[100];
        strcat(msg, "Error: Directory ");
        strcat(msg, dirName);
        strcat(msg, " nout found.\n");
        write(1, msg, strlen(msg));
        return;
    }

    DIR *dr = opendir(dirName);
    int fileExists = 0;
    if (dr == NULL)
    {
        write(1, "Could not open directory\n", 25);
        return;
    }
    while ((de = readdir(dr)) != NULL)
    {
        if (strstr(de->d_name, extension) != NULL)
        {
            write(1, de->d_name, strlen(de->d_name));
            write(1, "\n", 1);
            fileExists = 1;
        }
    }
    if (!fileExists)
    {
        char msg[100];
        strcat(msg, "No files with extension ");
        strcat(msg, extension);
        strcat(msg, " found in ");
        strcat(msg, dirName);
        strcat(msg, "\n");
        write(1, msg, strlen(msg));
    }
    closedir(dr);
}

void readFile(char *fileName)
{
    struct stat st = {0};
    if (stat(fileName, &st) == -1)
    {
        char msg[100];
        strcat(msg, "Error: File ");
        strcat(msg, fileName);
        strcat(msg, " not found.\n");
        write(1, msg, strlen(msg));
        return;
    }
    fileName[strlen(fileName) - 1] = '\0'; // Remove newline character
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file");
        return;
    }

    char buffer[100];
    int bytesRead = read(fd, buffer, 100);
    if (bytesRead == -1)
    {
        perror("Error reading file");
        return;
    }

    write(1, buffer, bytesRead);
    write(1, "\n", 1);
    close(fd);
}

void appendToFile(char *fileName, char *content)
{
    struct stat st = {0};
    if (stat(fileName, &st) == -1)
    {
        char msg[100];
        strcat(msg, "Error: File ");
        strcat(msg, fileName);
        strcat(msg, " not found.\n");
        write(1, msg, strlen(msg));
        return;
    }

    if (!(st.st_mode & S_IFREG) || isFileLocked(fileName))
    {
        char msg[100];
        strcat(msg, "Error: Cannot write to ");
        strcat(msg, fileName);
        strcat(msg, ". File is locked or read-only.\n");
        write(1, msg, strlen(msg));
        return;
    }

    content[strlen(content) - 1] = '\0'; // Remove newline character
    int fd = open(fileName, O_WRONLY | O_APPEND);
    if (fd == -1)
    {
        perror("Error opening file");
        return;
    }

    if (flock(fd, LOCK_EX) == -1)
    {
        perror("Error locking file");
        close(fd);
        return;
    }

    write(fd, "\n", 1);
    write(fd, content, strlen(content));

    if (flock(fd, LOCK_UN) == -1)
    {
        perror("Error unlocking file");
    }

    close(fd);
}


int main()
{
    char input[100];
    char **args = malloc(MAX_ARGS * sizeof(char *));

    while (1)
    {
        printf("Enter a command:\n");
        fgets(input, 100,  stdin);
        split(input, args);

        if (strcmp(args[0], "createDir") == 0)
        {
            createDir(args[1]);
        }
        else if (strcmp(args[0], "createFile") == 0)
        {
            creteFile(args[1]);
        }
        else if (strcmp(args[0], "listDir") == 0)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                perror("Error forking process");
                exit(1);
            }
            else if (pid == 0)
            {
                listDir(args[1]);
                exit(0);
            }
            else
            {
                waitpid(pid, NULL, 0);
            }        
        }
        else if (strcmp(args[0], "listFileByExtension") == 0)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                perror("Error forking process");
                exit(1);
            }
            else if (pid == 0)
            {
                listFileByExtension(args[1], args[2]);
                exit(0);
            }
            else
            {
                waitpid(pid, NULL, 0);
            }
        }
        else if (strcmp(args[0], "readFile") == 0)
        {
            readFile(args[1]);
        }
        else if (strcmp(args[0], "appendToFile") == 0)
        {
            appendToFile(args[1], args[2]);
        }
        else if (strcmp(args[0], "exit") == 0)
        {
            break;
        }
        else
        {
            printf("Invalid command\n");
        }
    }

    free(args);
    return 0;
}