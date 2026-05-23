#
# Makefile for NumbPipe
#
obj-m += numbpipe.o

numbpipe-objs := numb_pipe.o

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD)

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
		rm -f *.o *.mod.* .*.cmd
