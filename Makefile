NAME = webserv

CXX = c++

CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = main.cpp \
	RequestBody.cpp \
	HttpRequest.cpp \
	HttpResponse.cpp \
	LocationConfig.cpp \
	ServerConfig.cpp \
	ConfigParser.cpp \
	ServerManager.cpp \
	Client/Client.cpp \
	Client/RequestHandler.cpp \
	Client/RequestParser.cpp \
	Client/ResponseBuilder.cpp \
	cgi/CgiRequest.cpp

OBJDIR = objects
OBJS = $(SRCS:%.cpp=$(OBJDIR)/%.o)

DEPS = RequestBody.hpp \
	   HttpRequest.hpp \
	   HttpResponse.hpp \
	   LocationConfig.hpp \
	   ServerConfig.hpp \
	   ConfigParser.hpp \
	   ServerManager.hpp \
	   Client/Client.hpp \
	   Client/RequestHandler.hpp \
	   Client/RequestParser.hpp \
	   Client/ResponseBuilder.hpp \
	   cgi/CgiRequest.hpp

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