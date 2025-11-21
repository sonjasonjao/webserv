NAME	= webserv

CMD		= c++

STD		= -std=c++20

WARN	= -Wall -Werror -Wextra

DIR_INC	= include

DIR_SRC = srcs

FLAGS	= $(WARN) $(STD) -O0 -g -I$(DIR_INC) -MMD -MP

SRCS	= $(wildcard $(DIR_SRC)/*.cpp) main.cpp

OBJS	= $(SRCS:.cpp=.o)
DEPS	= $(OBJS:.o=.d)

all		: $(NAME)

$(NAME)	: $(OBJS)
	$(CMD) $(OBJS) -o $@

%.o		: %.cpp
	$(CMD) $(FLAGS) -c $< -o $@

clean	:
	rm -f $(OBJS) $(DEPS)

fclean	: clean
	rm -f $(NAME)

re		: fclean all

.PHONY	: all clean fclean re

-include $(DEPS)
