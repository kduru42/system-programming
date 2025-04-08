NAME = ipc_daemon
CC = gcc
CFLAGS = -Wall -Wextra -Werror -g
RM = rm -f

SRC = ipc_daemon.c

OBJ = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re