#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

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

void createDir(char *dirName)
{
    pid_t pid = fork();
    struct stat st = {0};
    dirName[strlen(dirName) - 1] = '\0'; // Remove newline character

    if (pid < 0)
    {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (stat(dirName, &st) == -1)
        {
            if (mkdir(dirName, 0700) == 0)
                printf("Directory %s created succesfully\n\n", dirName);
            else
                perror("Error creating directory");
        }
        else
        {
            printf("Directory already exists\n\n");
        }
    }
    else
    {
        waitpid(pid, NULL, 0);
    }
}


void creteFile(char *fileName)
{
    pid_t pid = fork();
    struct stat st = {0};
    fileName[strlen(fileName) - 1] = '\0'; // Remove newline character

    if (pid < 0)
    {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (stat(fileName, &st) == -1)
        {
            if (creat(fileName, 0700) == 0)
                printf("File %s created succesfully\n\n", fileName);
            else
                perror("Error creating file");
        }
        else
        {
            printf("File already exists\n\n");
        }
    }
    else
    {
        waitpid(pid, NULL, 0);
    }

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

        if (strcmp, args[0], "createDir")
        {
            createDir(args[1]);
        }
        else if (strcmp, args[0], "createFile")
        {
            creteFile(args[1]);
        }
        else if (strcmp, args[0], "exit")
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