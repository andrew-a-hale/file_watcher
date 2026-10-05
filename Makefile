default:
	gcc main.c -o main -Wall -fsanitize=address -O2

test: default
	./main . echo "change!"
