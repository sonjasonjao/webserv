NAME			= test_server

CMD				= c++

STD				= -std=c++20

WARN			= -Wall -Wextra -Werror

FLAGS			= $(WARN) $(STD) -O0 -g

# GoogleTest includes & sources
GTEST_URL		= https://github.com/google/googletest.git
GTEST_ROOT		= googletest
GTEST_DIR		= $(GTEST_ROOT)/googletest
GTEST_INC		= $(GTEST_DIR)/include
GTEST_SRC		= $(GTEST_DIR)/src/gtest-all.cc

# Project durectories
INC_DIR			= ../include
SRC_DIR			= ../srcs

# Project files
PROJECT_SRCS	= $(wildcard $(SRC_DIR)/*.cpp)
PROJECT_OBJS	= $(PROJECT_SRCS:.cpp=.o)

# Test sources
TEST_SOURCES	= \
	Parser_test.cpp\
	test_main.cpp

TEST_OBJECTS	= $(TEST_SOURCES:.cpp=.o)

all			: gtest $(NAME)

.PHONY		: gtest
gtest		: $(GTEST_ROOT)/.git

$(GTEST_ROOT)/.git :
	git clone --depth=1 $(GTEST_URL) $(GTEST_ROOT)

# Test executable (links your lib objects + gtest)
$(NAME): $(TEST_OBJECTS) $(PROJECT_OBJS) gtest-all.o
	$(CMD) $(FLAGS) 	$^ -pthread -o $@

# Generic rule to compile any .cpp -> .o
%.o			: %.cpp
	$(CMD) $(FLAGS) 	-I$(INC_DIR) -I$(GTEST_INC) -I$(GTEST_DIR) -c $< -o $@

gtest-all.o	: $(GTEST_SRC) | gtest
	$(CMD) $(FLAGS) 	-I$(GTEST_INC) -I$(GTEST_DIR) -c $< -o $@

clean		:
	rm -f $(TEST_OBJECTS) $(PROJECT_OBJS) gtest-all.o

fclean		: clean
	rm -f $(NAME)

re 			: fclean all

.PHONY: all gtest clean fclean re
