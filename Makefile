CC = gcc
CFLAGS = -Iinclude -Wall -fcolor-diagnostics -fansi-escape-codes -g
DEPS = include/djb2.h
BUILDDIR = build

_OBJ = djb2.o client.o server.o
OBJ = $(patsubst %,$(BUILDDIR)/%,$(_OBJ))
EXECS = $(BUILDDIR)/client $(BUILDDIR)/server

all: directories $(EXECS)

directories: $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BUILDDIR)/client: $(BUILDDIR)/client.o $(BUILDDIR)/djb2.o $(BUILDDIR)/utils.o
	$(CC) -o $@ $^ $(CFLAGS)

$(BUILDDIR)/server: $(BUILDDIR)/server.o $(BUILDDIR)/djb2.o $(BUILDDIR)/utils.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/*.out *~ core $(INCDIR)/*~ 
	rm -r $(BUILDDIR)
