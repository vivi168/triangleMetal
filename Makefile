CC := clang++
CFLAGS := -g -Wall -Wextra -std=c++17 -I./include
LDFLAGS := -framework Metal -framework Foundation -framework Quartz -lSDL2

EXE := triangle
SRC := main.cpp camera.cpp
OBJ := $(SRC:.cpp=.o)

all: $(EXE) shader.metallib

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

shader.metallib: shader.air
	xcrun -sdk macosx metallib shader.air -o shader.metallib

shader.air: shader.metal
	xcrun -sdk macosx metal -c shader.metal -o shader.air

.PHONY: clean
clean:
	rm -rf *.o *.air *.metallib $(EXE)
