CL65 ?= cl65
CCFLAGS ?= -Oirs
SOURCES = editor.c document.c machine.s

X16PERF.PRG: $(SOURCES) document.h
	$(CL65) -t cx16 --cpu 65c02 $(CCFLAGS) -m x16perfect.map -o $@ $(SOURCES)

clean:
	rm -f *.o tests/*.o x16perfect.map X16PERF.PRG STORAGE.PRG SAVETEST.PRG RETRIEVE.PRG

STORAGE.PRG: tests/storage.c document.c machine.s document.h
	$(CL65) -t cx16 --cpu 65c02 $(CCFLAGS) -o $@ $< document.c machine.s

SAVETEST.PRG: tests/save-replace.c document.c machine.s document.h
	$(CL65) -t cx16 --cpu 65c02 $(CCFLAGS) -o $@ $< document.c machine.s

RETRIEVE.PRG: tests/retrieve.c document.c machine.s document.h
	$(CL65) -t cx16 --cpu 65c02 $(CCFLAGS) -o $@ $< document.c machine.s
