all: integr.out collect.sh

integr.out: Multi.cpp
	gcc -pthread -o $@ $<

collect.sh:
	chmod +x collect.sh

clean: 
	rm integr.out

.PHONY: clean
