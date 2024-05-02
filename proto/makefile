make:
	make clean
	gcc main.c -o text-win-proto-gen
	./text-win-proto-gen ../config proto-gen-client.h --is-client
	./text-win-proto-gen ../config proto-gen-server.h --is-server

clean:
	clear
	rm -f \
		*.gch \
		proto-gen.h \
		proto-gen-client.h \
		proto-gen-server.h \
		debug-gen.h \
		debug-gen-client.h \
		debug-gen-server.h \
		default-gen.h

debug:
	make clean
	gcc main.c -g -o text-win-proto-gen
	rm -f debug-gen.h
	valgrind ./text-win-proto-gen \
		../config \
		debug-gen-client.h \
		--is-client
	
	./text-win-proto-gen ../config debug-gen-server.h --is-server
	cat debug-gen-client.h
	gcc debug-gen-client.h
	gcc debug-gen-server.h
