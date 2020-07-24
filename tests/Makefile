.SUFFIXES:

%32.c.o: %.c
	$(CC) -m32 -c $(CFLAGS) -o $@ $<

%32: %32.c.o
	$(LD) -lsystem $(LDFLAGS) -o $@ $<

%64: %32
	86x64 -o $@ -s symdefs.h $< > /dev/null

.PHONY: clean
clean:
	rm -rf *.dylib *64 *32 *~
