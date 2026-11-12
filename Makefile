CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
LDLIBS  ?= -lm -lpthread

TARGET  := partumtestkit
SRC     := src/main.c src/func.c src/workloads.c
OBJ     := $(SRC:.c=.o)

.PHONY: all quick clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c src/partum.h
	$(CC) $(CFLAGS) -c $< -o $@

quick: $(TARGET)
	./$(TARGET) --quick

clean:
	rm -f $(TARGET) $(OBJ)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
