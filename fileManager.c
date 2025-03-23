#include "fileManager.h"

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        write(1, "Usage: fileManager <command> [arguments] \n", 43);
        return 1;
    }

    if (strcmp(argv[1], "fileManager") == 0)
    {
        write(1, "Usage: fileManager <command> [arguments] \n\n", 44);
        write(1, "Commands:\n", 11);
        write(1, "  createDir <folderName>      - Create a new directory\n", 56);
        write(1, "  createFile <fileName>       - Create a new file\n", 51);
        write(1, "  listDir <folderName>        - List all files in a directory\n", 63);
        write(1, "  listFileByExtension <folderName> <extension>        - List files with spesific extension\n", 92);
        write(1, "  readFile <fileName>         - Read a file's content\n", 55);
        write(1, "  appendToFile <fileName> <new content>       - Append content to a file\n", 74);
        write(1, "  deleteFile <fileName>       - Delete a file\n", 47);
        write(1, "  deleteDir <folderName>      - Delete an empty directory\n", 59);
        write(1, "  showLogs                    - Display operation logs\n", 56);
        return 1;
    }


    if (strcmp(argv[1], "createDir") == 0)
    {
        createDir(argv[2]);
    }
    else if (strcmp(argv[1], "createFile") == 0)
    {
        creteFile(argv[2]);
    }
    else if (strcmp(argv[1], "listDir") == 0)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            my_perror("Error forking process");
            exit(1);
        }
        else if (pid == 0)
        {
            listDir(argv[2]);
            exit(0);
        }
        else
        {
            waitpid(pid, NULL, 0);
        }        
    }
    else if (strcmp(argv[1], "listFilesByExtension") == 0)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            my_perror("Error forking process");
            exit(1);
        }
        else if (pid == 0)
        {
            listFileByExtension(argv[2], argv[3]);
            exit(0);
        }
        else
        {
            waitpid(pid, NULL, 0);
        }
    }
    else if (strcmp(argv[1], "readFile") == 0)
    {
        readFile(argv[2]);
    }
    else if (strcmp(argv[1], "appendToFile") == 0)
    {
        char msg[100];
        msg[0] = '\0';
        strcat(msg, argv[3]);
        for (int i = 4; i < argc; i++)
        {
            strcat(msg, " ");
            strcat(msg, argv[i]);
        }
        appendToFile(argv[2], msg);
    }
    else if (strcmp(argv[1], "deleteFile") == 0)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            my_perror("Error forking process");
            exit(1);
        }
        else if (pid == 0)
        {
            deleteFile(argv[2]);
            exit(0);
        }
        else
        {
            waitpid(pid, NULL, 0);
        }
    }
    else if (strcmp(argv[1], "deleteDir") == 0)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            my_perror("Error forking process");
            exit(1);
        }
        else if (pid == 0)
        {
            deleteDir(argv[2]);
            exit(0);
        }
        else
        {
            waitpid(pid, NULL, 0);
        }
    }
    else if (strcmp(argv[1], "showLogs") == 0)
    {
        showLogs();
    }
    else
    {
        write(1, "Invalid command\n", 16);
    }
    return 0;
}