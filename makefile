s-talk: s-talk.o LIST.o
	gcc -pthread -o s-talk s-talk.o LIST.o
    
s-talk.o: s-talk.c LIST.c
	gcc -c s-talk.c

LIST.o: LIST.c LIST.h
	gcc -c LIST.c
    
clean:
	rm s-talk.o s-talk LIST.o
