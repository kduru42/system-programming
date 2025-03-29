#ifndef FILEMANAGER_H
#define FILEMANAGER_H

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
#include <errno.h>

void my_perror(const char *msg);
int isFileLocked(const char *fileName);
char *getTimestamp();
void logOperation(char *msg);
void createDir(char *dirName);
void createFile(char *fileName);
void listDir(char *dirName);
void listFileByExtension(char *dirName, char *extension);
void readFile(char *fileName);
void appendToFile(char *fileName, char *content);
void deleteDir(char *dirName);
void deleteFile(char *fileName);
void showLogs();


#endif