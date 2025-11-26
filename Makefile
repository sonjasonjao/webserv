# ---------------------------------------------------------------------------- #
NAME		:= webserv

CXX			:= c++
CXX_FLAGS	:= -Wall -Wextra -Werror -std=c++20 -MMD
DEBUG_FLAGS	:= -g
# ---------------------------------------------------------------------------- #
INC_DIR		:= ./include
SRC_DIR		:= ./srcs
OBJ_DIR		:= ./obj
# ---------------------------------------------------------------------------- #
SRC	:=	$(SRC_DIR)/main.cpp				\
		$(SRC_DIR)/CustomException.cpp	\
		$(SRC_DIR)/Log.cpp				\
		$(SRC_DIR)/Parser.cpp			\
		$(SRC_DIR)/Server.cpp			\
		$(SRC_DIR)/Request.cpp

OBJ	:=	$(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRC))
DEP	:=	$(OBJ:.o=.d)

.SECONDARY: $(OBJ)
# ---------------------------------------------------------------------------- #
all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXX_FLAGS) -I $(INC_DIR) $(OBJ) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXX_FLAGS) -I $(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	mkdir $(OBJ_DIR)
# ---------------------------------------------------------------------------- #
clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all
# ---------------------------------------------------------------------------- #
run: $(NAME)
	./$(NAME) ./config_files/default.config

gdb: $(NAME)
	gdb -tui $(NAME)

ddb: debug gdb
# ---------------------------------------------------------------------------- #
debug: CXX_FLAGS += $(DEBUG_FLAGS)
debug: fclean all
# ---------------------------------------------------------------------------- #
.PHONY: all clean fclean re debug run gdb ddb
# ---------------------------------------------------------------------------- #
-include $(DEP)
# ---------------------------------------------------------------------------- #
