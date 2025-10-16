# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Iinclude -Wall -Wextra

# Libraries (zlib + OpenSSL)
LIBS = -lz -lssl -lcrypto

# Source files and objects
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)

# Output executable
OUT = bit

# Default build rule
$(OUT): $(OBJ)
	$(CXX) $(OBJ) -o $(OUT) $(LIBS)

# Rule for building object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJ) $(OUT)
