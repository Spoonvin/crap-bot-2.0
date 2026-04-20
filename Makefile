# Compiler
CXX = g++

# Flags
CXXFLAGS = -g -Og -std=c++17 -Iinclude $(shell pkg-config --cflags sdl3 sdl3-image)

# Linker flags
LDFLAGS = $(shell pkg-config --libs sdl3 sdl3-image)

# Files
SRC = $(shell find src -name "*.cc")
OBJ = $(SRC:.cc=.o)

# Output
TARGET = app

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compile
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJ) $(TARGET)