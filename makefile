default: hermes.c c2.c test.c
	gcc -fPIC -shared hermes.c -o hermes.so
	gcc c2.c -o c2
	gcc test.c -o test
	patchelf --add-needed ./hermes.so test

clean:
	rm -f hermes.so c2 test *~
	killall test

