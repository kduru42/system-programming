#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


int main()
{
    char *fifo_name = "/tmp/fifo1";
    char *fifo_name2 = "/tmp/fifo2";

    unlink(fifo_name);
    unlink(fifo_name2);

    return 0;
}