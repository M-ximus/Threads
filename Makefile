all: integr.out

integr.out: Multi.cpp
	gcc -pthread -o $@ $<

clean: 
	rm integr.out

.PHONY: clean
