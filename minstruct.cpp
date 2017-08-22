#include "minstruct.h"

MInstruct::MInstruct()
{
}

/* Initializer*/
MInstruct::MInstruct(int a, int b, int c, int d, int e, int f, int s):
    Opcode(a), rs(b), rt(c), rd(d), shamt(e), funct(f), immediate(s)
{
    taken = false;
}
