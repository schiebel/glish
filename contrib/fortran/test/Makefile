
all: f77client

f77client: main.o fort.o
	g++ -o $@ $^ -L../../../lib/i686-unknown-linux -lglish -lsos -lnpd -lg2c -lm

main.o: main.cc
	g++ -I../../../include -c main.cc

fort.o: fort.f
	g77 -c $<

