NAME = webserv

CXX = c++

CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = main.cpp \
	HttpRequest.cpp \
	HttpResponse.cpp \
	LocationConfig.cpp \
	ServerConfig.cpp \
	ConfigParser.cpp \
	Client.cpp 

OBJDIR = objects
OBJS = $(SRCS:%.cpp=$(OBJDIR)/%.o)

DEPS = HttpRequest.hpp \
	   HttpResponse.hpp \
	   LocationConfig.hpp \
	   ServerConfig.hpp \
	   ConfigParser.hpp \
	   Client.hpp 

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: %.cpp $(DEPS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re