out.wav : drummer
	./drummer

drummer : drummer.c
	gcc -o drummer drummer.c -Wall -pedantic -O2
