.SUFFIXES:

OBJS = printf32-64.dylib wrapper.asm.o libinterpose.dylib

wrapper: $(OBJS) Makefile
	$(LD) $(LDFLAGS) -pagezero_size 0x1000 -lsystem -e _main_wrapper -o $@ $(OBJS)

%.c.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

%32.c.o: %.c Makefile
	$(CC) $(CFLAGS) -m32 -c -o $@ $<

printf32-64.dylib: printf32-64-abi macho-tool Makefile
	macho-tool convert --archive DYLIB $< $@

printf32-64: printf32 macho-tool Makefile
	macho-tool transform $< $@

printf32-64-abi: printf32-64 libabiconv.dylib macho-tool
	macho-tool modify --insert load-dylib,name="libabiconv.dylib" $< $@

printf32: printf32.c.o Makefile
	$(LD) $(LDFLAGS) -lsystem -o $@ $<

macho-tool:
	$(MAKE) -C ..

%.asm.o: %.asm Makefile
	nasm -f macho64 -o $@ $<

ABICONV_OBJS = abiconv.asm.o
INTERPOSE_OBJS = interpose.c.o

libinterpose.dylib: $(INTERPOSE_OBJS) Makefile
	$(CC) $(LDFLAGS) -dynamiclib -o $@ $(INTERPOSE_OBJS)

libabiconv.dylib: $(ABICONV_OBJS) Makefile
	$(CC) $(LDFLAGS) -dynamiclib -o $@ $(ABICONV_OBJS)

STATIC_INTERPOSE_SYMS = _printf
.PHONY: static-interpose
static-interpose:
	echo stub
