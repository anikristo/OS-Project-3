all: ts_indexgen librwlock.a app

ts_indexgen: 
	gcc -Wall -o ts_indexgen ts_indexgen.c -lpthread

librwlock.a: 	
	gcc -Wall -c rwlock.c 
	ar -cvq librwlock.a rwlock.o
	ranlib librwlock.a

app: 	
	gcc -Wall -o app ../supplied/app.c -I. -L. -lrwlock -lpthread

clean: 
	rm -fr required/*.o required/*.a required/*~ required/a.out required/app required/ts_indexgen  required/rwlock.o required/librwlock.a

