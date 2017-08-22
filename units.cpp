#include "units.h"
#include <iostream>

using namespace std;

RS::RS()
{

    Qj = INT_MAX;
    Qk = INT_MAX;
    Vj = INT_MAX;
    Vk = INT_MAX;

    Opcode = 0;
    result = 0;
    busy = false;
    done = false;
    dest = 0;
    A = 0;
}

/* Initializer*/
RS::RS(int a, int b, int c, int d, int e, int f, int g):
    Opcode(a), Qj(b), Qk(c), Vj(d), Vk(e), dest(f), A(g)
{

    Qj = INT_MAX;
    Qk = INT_MAX;
    Vj = INT_MAX;
    Vk = INT_MAX;

    result = 0;
    busy = false;
    done = false;
    dest = 0;
}

void RS::print()
{
    cout << "busy: "    << busy     << "|"
         << "Opcode: "  << Opcode   << "|"
         << "Qj: "      << Qj       << "|"
         << "Qk: "      << Qk       << "|"
         << "Vj: "      << Vj       << "|"
         << "Vk: "      << Vk       << "|"
         << "dest: "    << dest     << "|"
         << "A: "       << A        << "|"
         << "Done: "    << done     << "|"
         << "result: "  << result   << endl;
}



ROB::ROB()
{
    busy = false;
    ready = false;
    value = 0;

    instruction = -1;
    state = 0;
    destination = 0;
    value = 0;

    outcome = -1;
    correct = false;
    oldpc = -1;
}

/* Initializer*/
ROB::ROB(int a, int b, int c, int d):
    instruction(a), state(b), destination(c), value(d)
{
    busy = false;
    ready = false;
    value = 0;
    A = -1;
    done = false;

    outcome = -1;
    correct = false;
    oldpc = -1;

}

void ROB::print()
{
    cout << "busy: "  << busy << "|"
         << "ready: " << ready << "|"
         << "instruction: "<< instruction << "|"
         << "state: " << state << "|"
         << "destination: " << destination << "|"
         << "Address: "<< A  << "|"
         << "Correct: "<<correct<< "|"
         << "value: " << value;
}

RF::RF()
{
    busy = false;
    reorder = 0;
}

/* Initializer*/
RF::RF(int a):
    reorder(a)
{
    busy = false;
    reorder = 0;
}

void RF::print()
{
    cout << "busy: "  << busy << "|"
         << "reorder: " << reorder << endl;
}

BTB::BTB()
{
}

/* Initializer*/
BTB::BTB(int a,int b, int c, int d):
    PC(a), target(b), predictor(c), real(d)
{}

void BTB::print()
{
    cout << "PC: "  << PC << "|"
         << "target: " << target << "|"

         << "predictor: "<< predictor
         << "real address: "<< real <<endl;

}
