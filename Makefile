SRCS = src/main.c \
       src/process.c \
       src/join.c \
       src/bootstrap.c \
       src/leave.c \
       src/search.c \
       src/server.c

OBJS = $(SRCS:.c=.o)

HEADERS = headers/header.h

CC = cc
CFLAGS = -Wall -Wextra -Werror

NAME = ddt
BOOT_SERVER = boot_server

all: $(NAME)

$(NAME): $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -Iheaders -o $(NAME) $(OBJS)

$(BOOT_SERVER): src/boot_server.c $(HEADERS)
	$(CC) $(CFLAGS) -Iheaders -o $(BOOT_SERVER) src/boot_server.c

bootserver: $(BOOT_SERVER)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -Iheaders -c $< -o $@

clean:
	rm -f $(OBJS) src/boot_server.o

fclean: clean
	rm -f $(NAME) $(BOOT_SERVER)

re: fclean all

.PHONY: all clean fclean re bootserver
