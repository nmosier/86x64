.SUFFIXES:

wrapper: printf32-64.dylib wrapper.c.o
	$(LD) -lsystem -e _main_wrapper -o $@ $^

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%32.c.o: %.c
	$(CC) $(CFLAGS) -m32 -c -o $@ $<

printf32-64.dylib: printf32-64 macho-tool
	macho-tool convert --archive DYLIB $< $@

printf32-64: printf32 macho-tool
	macho-tool transform $< $@

printf32: printf32.c.o
	$(LD) $(LDFLAGS) -lsystem -o $@ $<

macho-tool:
	$(MAKE) -C ..
