default:
	gcc main.c -o fw -Wall -O3

test: default
	./main . echo "change!"
