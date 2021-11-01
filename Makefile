###################################
# Build the Nova Threefry code    #
# By Scott Pakin <pakin@lanl.gov> #
###################################

SCDIR = $(HOME)/src/SingularComputingMaterialProvidedToLANL/System\ Code
CPPFLAGS = -I$(SCDIR)
CFLAGS = -g
LDFLAGS = -L$(SCDIR)
LIBS = -lS1 -lm

all: nova-threefry

nova-threefry.o: nova-threefry.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c nova-threefry.c

nova-threefry: nova-threefry.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o nova-threefry nova-threefry.o $(LIBS)

clean:
	$(RM) nova-threefry nova-threefry.o

.PHONY: all clean
