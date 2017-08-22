#ifndef UNITS_H
#define UNITS_H

#include <stdlib.h>
#include <string>
#include <climits>

#define NONE	 0
#define ISSUE	 1
#define EXECUTE	 2
#define WRITE	 3
#define COMMIT   4
#define EXEL1    5	// Load Step1
#define EXEL2    6	// Load Step2

using std::string;

class RS /* Reservation Station*/
{
public:
    //name is unneeded since we can use the index of the array
    bool busy;   	 /*    busy   */
    int  Opcode;         /* the marco */

    /* Note:ROB's index start from 1! */
    int  Qj;             /* the int here indicates which entry in ROB would produce the operands*/
    int  Qk;             /* the int here indicates which entry in ROB would produce the operands*/
    int  Vj;             /* the int here is the exact value*/
    int  Vk;          	 /* the int here is the exact value*/

    int  dest;           /* indicate the ROB entry as the destination */
    int  A;		 /* Addr for L/S, set to 0 initially and -1 for incs not L/S 	*/

    int result;		 /* tmp for result*/
    bool done;		 /* flag for checking whether calculation is done*/

public:
    RS();
    RS(int,int,int,int,int,int,int) ;
    void print();
};

class ROB /* Reorder Buffer*/
{
public:
    bool busy;		 /* busy */
    bool ready;   	 /* ready*/
    int  instruction;	 /* index of the instruction in IQ */
    int  state;		 /* using marco as the 5-stage state*/
    int  destination;    /* the int here indicate the entry of register file */
    int  value;		 /* the value computed */

    int  outcome;	 /* Only for J_Type*/	
    int  oldpc;		 /* Only for J_Type, thi record the address of the Jump itself*/	
    bool correct;	 /* Only for J_Type*/	

    int A;		 /* These 2 are for L/S only*/
    bool done;		

public:
    ROB();
    ROB(int,int,int,int);
    void print();
};

class RF /* Register file*/
{
public:
    bool busy;	 /* busy */
    int  reorder;	 /* index of the instruction in ROB which will write value into it */

public:
    RF();
    RF(int);
    void print();
};

class BTB /* Branch target buffer*/
{
public:
    int  PC;		/* PC address,716 i.e..*/
    int  target;	/* Predicted PC */
    int  predictor;	/* The 1-bit preditor */
    int  real;		/* The address, real target*/
public:
    BTB();
    BTB(int,int,int,int);
    void print();
};



#endif // UNITS_H

