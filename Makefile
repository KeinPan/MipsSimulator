all:
	g++ main.cpp minstruct.cpp units.cpp -o MIPSsim -I .
cl:
	rm -f *.o MIPSsim
