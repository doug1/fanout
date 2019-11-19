
fanout: fanout.c
	gcc -Wall -o fanout fanout.c

clean:
	rm -f fanout

indent:
	indent -l80 -sob fanout.c

