#ifndef MINSTRUCT_H
#define MINSTRUCT_H

#include <stdlib.h>
#include <string>

using std::string;

class MInstruct /* For simplicity, we don't apply Inheritage*/
{
public:

    /* The computed value for rs, rt,rd are all the index of registers
     * As their length are fixed to 5 or 6, for immediate we always use
     * 10's to represents them, so all attributes are ints          */

    int Opcode;         /* 6 bits */
    int rs;             /* 5 bits , J-type don't possess*/
    int rt;             /* 5 bits , J-type don't possess*/
    int rd;             /* 5 bits , R-type only*/
    int shamt;          /* 5 bits , R-type only*/
    int funct;          /* 5 bits , R-type only*/
    int immediate;/* 16bits for I-type, "" for R-type, 26bits for J-type(used as Offset), but usually trans to 10's*/
    // int type;        /* Unnecessary in face of known Opcode*/
    int index;		/* To record index in ins_set*/
    bool taken;		/* To record the prediction at IF stage */ 

public:
    MInstruct();
    MInstruct(int a, int b, int c, int d, int e, int f, int s);
};

#endif // MINSTRUCT_H

