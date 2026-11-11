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

all: $(NAME)

$(NAME): $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -Iheaders -o $(NAME) $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -Iheaders -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re