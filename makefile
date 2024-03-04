all: client fs disk

clean:
	rm *.o

client: client.o
	gcc client.o -o client

fs: fs.o
	gcc fs.o -o fs

disk: disk.o
	gcc disk.o -o disk

client.o: client.c
	gcc -c client.c

fs.o: fs.c
	gcc -c fs.c

disk.o: disk.c
	gcc -c disk.c