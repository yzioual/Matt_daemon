NAME := Matt_daemon

SRC_DIR := src
OBJ_DIR := obj

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

CXX := c++
STD ?= c++23
CXXFLAGS := -Wall -Wextra -Werror -std=$(STD) -I$(SRC_DIR) -g -MMD -MP
LDFLAGS :=
LDLIBS :=

RM := rm -f

all: $(NAME)

$(NAME): $(OBJS)
	@echo "[Matt_daemon] Linking $(NAME)..."
	@$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $(NAME)
	@echo "[Matt_daemon] $(NAME) build complete!"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "[Matt_daemon] Cleaning object files..."
	@rm -rf $(OBJ_DIR)

fclean: clean
	@echo "[Matt_daemon] Removing $(NAME)..."
	@$(RM) $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
