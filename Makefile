SRCS = src/main.c \
		src/process.c
		src/join.c
		src/bootstrap.c
		src/leave.c
		src/search.c
		src/server.c

OBJS = $(SRCS:.c=.o)

HEADERS = headers/header.h

COMPILER = cc

CFLAGS = -Wall -Wextra -Werror

NAME = ddt

$(NAME): $(OBJS) $(HEADERS)
	$(COMPILER) $(CFLAGS) -o $(NAME) $(OBJS) -I../headers

all: $(NAME)

clean:
	rm -f $(OBJS) $(NAME)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re