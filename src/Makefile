CFLAGS = -g -Wall -Wextra -Werror -fsanitize=address,leak,object-size,bounds-strict,undefined -fsanitize-address-use-after-scope

all: mysh

mysh: mysh.o builtins.o commands.o variables.o shell_io.o
	gcc ${CFLAGS} -o $@ $^

%.o: %.c builtins.h commands.h variables.h shell_io.h
	gcc ${CFLAGS} -c $<

clean:
	rm -f *.o mysh