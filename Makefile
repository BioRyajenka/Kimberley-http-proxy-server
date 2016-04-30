# Declaration of variables
CC = g++
CC_FLAGS = -w -g -O0 -std=c++14 -rdynamic
LIBS = -lpthread

# File names
EXEC = run
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# Main target
$(EXEC): $(OBJECTS)
	$(CC) $(LIBS) $(OBJECTS) -o $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CC) -c $(CC_FLAGS) $< -o $@

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)