SRCS = main.c utils.c

OBJS = $(SRCS:.c=.o)

HEADERS = header.h

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