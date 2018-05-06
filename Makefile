all:
	make skcc


CC = gcc
CFLAGS = -std=c11
TMPDIR = tmp

skcc: tmp tmp/error.o tmp/utf8.o tmp/file.o tmp/string.o tmp/lex.o tmp/main.o
	${CC} ${CFLAGS} -o skcc tmp/error.o tmp/utf8.o tmp/file.o tmp/string.o tmp/lex.o tmp/main.o

tmp:
	mkdir tmp

tmp/error.o: tmp error.c
	${CC} ${CFLAGS} -c -o tmp/error.o error.c

tmp/utf8.o: tmp utf8.c
	${CC} ${CFLAGS} -c -o tmp/utf8.o utf8.c

tmp/file.o: tmp file.c
	${CC} ${CFLAGS} -c -o tmp/file.o file.c

tmp/string.o: tmp string.c
	${CC} ${CFLAGS} -c -o tmp/string.o string.c

tmp/lex.o: tmp lex.c
	${CC} ${CFLAGS} -c -o tmp/lex.o lex.c

tmp/main.o: tmp main.c
	${CC} ${CFLAGS} -c -o tmp/main.o main.c


test: tmp/lex_test
	./tmp/lex_test tests/lex/cases/header_name.c tests/lex/cases/header_name.in
	./tmp/lex_test tests/lex/cases/identifier.c tests/lex/cases/identifier.in
	./tmp/lex_test tests/lex/cases/pp_number.c tests/lex/cases/pp_number.in
	./tmp/lex_test tests/lex/cases/character_constant.c tests/lex/cases/character_constant.in
	./tmp/lex_test tests/lex/cases/string_literal.c tests/lex/cases/string_literal.in
	./tmp/lex_test tests/lex/cases/punctuator.c tests/lex/cases/punctuator.in
	./tmp/lex_test tests/lex/cases/comment.c tests/lex/cases/comment.in
	./tmp/lex_test tests/lex/cases/hello_world.c tests/lex/cases/hello_world.in

tmp/lex_test: tmp tmp/error.o tmp/utf8.o tmp/file.o tmp/string.o tmp/lex.o tmp/lex_driver.o
	${CC} ${CFLAGS} -o tmp/lex_test tmp/error.o tmp/utf8.o tmp/file.o tmp/string.o tmp/lex.o tmp/lex_driver.o

tmp/lex_driver.o: tmp tests/lex/driver.c
	${CC} ${CFLAGS} -c -o tmp/lex_driver.o tests/lex/driver.c


clean:
	rm -rf skcc
	rm -rf tmp
