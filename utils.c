#include "fileManager.h"

void my_perror(const char *msg)
{
    char errorMsg[256];
    int len = 0;

    // Copy the custom message
    while (msg[len] != '\0')
    {
        errorMsg[len] = msg[len];
        len++;
    }

    // Add ": " separator
    errorMsg[len++] = ':';
    errorMsg[len++] = ' ';

    // Copy the error description
    const char *errStr = strerror(errno);
    int errLen = 0;
    while (errStr[errLen] != '\0')
    {
        errorMsg[len++] = errStr[errLen++];
    }

    // Add newline
    errorMsg[len++] = '\n';
    errorMsg[len] = '\0';

    // Print using `write()`
    write(2, errorMsg, len); // File descriptor 2 = stderr
}


int isFileLocked(const char *fileName)
{
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        my_perror("Error opening file");
        return -1;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;  // Check for write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;         // Check entire file

    if (fcntl(fd, F_GETLK, &lock) == -1)
    {
        my_perror("fcntl error");
        close(fd);
        return -1;
    }

    close(fd);
    return (lock.l_type == F_UNLCK) ? 0 : 1; // 0 = not locked, 1 = locked
}

char *getTimestamp() {
    static char buffer[30];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", timeinfo);

    return buffer;
}

void logOperation(char *message)
{
    int fd = open("log.txt", O_WRONLY | O_APPEND | O_CREAT, 0700);
    if (fd == -1)
    {
        my_perror("Error opening log file");
        return;
    }

    write(fd, message, strlen(message));
    write(fd, "\n", 1);

    close(fd);
}


void createDir(char *dirName)
{
    struct stat st = {0};

    if (stat(dirName, &st) == -1)
    {
        if (mkdir(dirName, 0700) == 0)
        {
            write(1, "Directory created successfully\n\n", 32);
            char *timestamp = getTimestamp();
            char msg[200];
            msg[0] = '\0';
            strcat(msg, timestamp);
            strcat(msg, "Directory ");
            strcat(msg, dirName);
            strcat(msg, " created successfully");
            logOperation(msg);
        }
        else
            my_perror("Error creating directory");
    }
    else
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: Directory ");
        strcat(msg, dirName);
        strcat(msg, " already exists\n\n");
        write(1, msg, strlen(msg));
    }
}


void creteFile(char *fileName)
{
    struct stat st = {0};

    if (stat(fileName, &st) == -1)
    {
        int fd = open(fileName, O_CREAT | O_WRONLY, 0700);
        if (fd != -1)
        {
            char *timestamp = getTimestamp();
            write(1, "File created successfully\n\n", 27);
            write(fd, timestamp, strlen(timestamp));
            char msg[200];
            msg[0] = '\0';
            strcat(msg, timestamp);
            strcat(msg, "File ");
            strcat(msg, fileName);
            strcat(msg, " created successfully");
            logOperation(msg);         
            close(fd);
        }
        else
        {
            my_perror("Error creating file");
        }
    }
    else
    {
        char msg[100];
        msg[0] = '\0';
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
    if (stat(dirName, &st) == -1)
    {   
        char msg[100];
        msg[0] = '\0';
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
    char msg[200];
    msg[0] = '\0';
    strcat(msg, getTimestamp());
    strcat(msg, "Directory ");
    strcat(msg, dirName);
    strcat(msg, " listed successfully");
    logOperation(msg);
    closedir(dr);
}

void listFileByExtension(char *dirName, char *extension)
{
    struct dirent *de;
    struct stat st = {0};
    if (stat(dirName, &st) == -1)
    {   
        char msg[100];
        msg[0] = '\0';
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
        msg[0] = '\0';
        strcat(msg, "No files with extension ");
        strcat(msg, extension);
        strcat(msg, " found in ");
        strcat(msg, dirName);
        strcat(msg, "\n");
        write(1, msg, strlen(msg));
    }
    else
    {
        char msg[200];
        msg[0] = '\0';
        strcat(msg, getTimestamp());
        strcat(msg, "Files with extension ");
        strcat(msg, extension);
        strcat(msg, " listed successfully");
        logOperation(msg);
    }
    closedir(dr);
}

void readFile(char *fileName)
{
    struct stat st = {0};
    if (stat(fileName, &st) == -1)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: File ");
        strcat(msg, fileName);
        strcat(msg, " not found.\n");
        write(1, msg, strlen(msg));
        return;
    }
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        my_perror("Error opening file");
        return;
    }

    char buffer[100];
    int bytesRead = read(fd, buffer, 100);
    if (bytesRead == -1)
    {
        my_perror("Error reading file");
        return;
    }

    write(1, buffer, bytesRead);
    write(1, "\n", 1);
    char msg[200];
    msg[0] = '\0';
    strcat(msg, getTimestamp());
    strcat(msg, "File ");
    strcat(msg, fileName);
    strcat(msg, " read successfully");
    logOperation(msg);
    close(fd);
}

void deleteFile(char *fileName)
{
    struct stat st = {0};
    if (stat(fileName, &st) == -1)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: File ");
        strcat(msg, fileName);
        strcat(msg, " not found.\n");
        write(1, msg, strlen(msg));
        return;
    }

    if (unlink(fileName) == 0)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "File ");
        strcat(msg, fileName);
        strcat(msg, " deleted successfully\n");
        write(1, msg, strlen(msg));
        char msg2[100];
        msg2[0] = '\0';
        strcat(msg2, getTimestamp());
        strcat(msg2, "File ");
        strcat(msg2, fileName);
        strcat(msg2, " deleted successfully");
        logOperation(msg2);
        return;
    }
    else
    {
        my_perror("Error deleting file");
    }
}

int isDirEmpty(char *dirName)
{
    DIR *dr = opendir(dirName);
    if (dr == NULL)
    {
        write(1, "Could not open directory\n", 25);
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(dr)) != NULL)
    {
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
        {
            closedir(dr);
            return 0;
        }
    }
    closedir(dr);
    return 1;
}

void deleteDir(char *dirName)
{
    struct stat st = {0};
    if (stat(dirName, &st) == -1)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: Directory ");
        strcat(msg, dirName);
        strcat(msg, " not found.\n");
        write(1, msg, strlen(msg));
        return;
    }

    if (!isDirEmpty(dirName))
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: Directory ");
        strcat(msg, dirName);
        strcat(msg, " is not empty.\n");
        write(1, msg, strlen(msg));
        return;
    }

    if (rmdir(dirName) == 0)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Directory ");
        strcat(msg, dirName);
        strcat(msg, " deleted successfully\n");
        write(1, msg, strlen(msg));
        char msg2[100];
        msg2[0] = '\0';
        strcat(msg2, getTimestamp());
        strcat(msg2, "Directory ");
        strcat(msg2, dirName);
        strcat(msg2, " deleted successfully");
        logOperation(msg2);
        return;
    }
    else
    {
        my_perror("Error deleting directory");
    }
}

void appendToFile(char *fileName, char *content)
{
    struct stat st = {0};
    if (stat(fileName, &st) == -1)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: File ");
        strcat(msg, fileName);
        strcat(msg, " not found.\n");
        write(1, msg, strlen(msg));
        return;
    }

    if (!(st.st_mode & S_IFREG) || isFileLocked(fileName))
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, "Error: Cannot write to ");
        strcat(msg, fileName);
        strcat(msg, ". File is locked or read-only.\n");
        write(1, msg, strlen(msg));
        return;
    }

    int fd = open(fileName, O_WRONLY | O_APPEND);
    if (fd == -1)
    {
        my_perror("Error opening file");
        return;
    }

    if (flock(fd, LOCK_EX) == -1)
    {
        my_perror("Error locking file");
        close(fd);
        return;
    }

    write(fd, "\n", 1);
    write(fd, content, strlen(content));

    if (flock(fd, LOCK_UN) == -1)
    {
        my_perror("Error unlocking file");
    }

    char msg[100];
    msg[0] = '\0';
    strcat(msg, getTimestamp());
    strcat(msg, "Content appended to file ");
    strcat(msg, fileName);
    strcat(msg, " successfully");
    logOperation(msg);

    close(fd);
}

void showLogs()
{
    int fd = open("log.txt", O_RDONLY);
    if (fd == -1)
    {
        my_perror("Error opening log file");
        return;
    }

    char buffer[500];
    int bytesRead;
    while ((bytesRead = read(fd, buffer, 100)) > 0)
    {
        write(1, buffer, bytesRead);
    }

    close(fd);
}