#include <climits>
#include <iostream>
#include <fstream>
#include <minstruct.h>
#include <units.h>
#include <vector>
#include <queue>

/*----------------------------MARCO FOR OPTYPE-----------------------*/
#define TYPE_FP           0 // Break is also considered as FP op,make one more check before Issue
#define TYPE_B            1
#define TYPE_L            2
#define TYPE_S            3
#define TYPE_J            4


/*----------------------------MARCO FOR OPCODE-----------------------*/
#define OP_SW        101011
#define OP_LW        100011
#define OP_J     /*0000*/10
#define OP_BEQ   /*000*/100
#define OP_BNE   /*000*/101
#define REGIMM   /*00000*/1 // for BGEZ & BLTZ
#define OP_BGTZ  /*000*/111
#define OP_BLEZ  /*000*/110
#define OP_ADDI  /*00*/1000
#define OP_ADDIU /*00*/1001
#define OP_SLTI  /*00*/1010
#define SPECIAL  /*00000*/0 /* for BREAK, SLT, SLTU, SLL, SRL, SRA, SUB,
                             * SUBU, ADD, ADDU, AND, OR, XOR, NOR, NOP*/


#define FC_BREAK /*00*/1101
#define FC_SLT       101010
#define FC_SLTU      101011
#define FC_SLL   /*00000*/0
#define FC_SRL   /*0000*/10
#define FC_SRA   /*0000*/11
#define FC_SUB       100010
#define FC_SUBU      100011
#define FC_ADD       100000
#define FC_ADDU      100001
#define FC_AND       100100
#define FC_OR        100101
#define FC_XOR       100110
#define FC_NOR       100111
//No NOP's MARCO, since SLL r0,r0,0 is expressed as NOP
using namespace std;

/*----------------------------------m:n--------------------------------------*/
int option_n = 0;
int option_m = 0;
bool IsNum(char c)
{
    if( (c == '0')||(c == '1')||(c == '2')||(c == '3')||(c == '4')||(c == '5')||(c == '6')||(c == '7')||(c == '8')||(c == '9') )
        return true;
    else
        return false;
}


/*---------------------------------FLAGS-------------------------------------*/
void LRUTail(int );
// the line between data and code
int line = 0;

// the flag to check whteher ROB's head has changed
bool flag_whether_ROB_changed = false;

// the flag to check whteher ROB's need to be flushed
bool flag_whether_ROB_flush   = false;

// the flag to see whether we meet the end (break)
bool flag_whether_Stop        = false;

// the global var : PC
int PC = 600;

// Given the address, find out the index of MeM
int addr_trans(int a)
{
    return (a-716)/4;
}
// Given the Opcode , find out the operatuon type
int type_trans(int marco)
{
    switch(marco)
    {
    case OP_SW:
          return TYPE_S;
    case OP_LW:
          return TYPE_L;

    case OP_J:
        return TYPE_J;
    case OP_BEQ:
    case OP_BNE:
    case OP_BGTZ:
    case OP_BLEZ:
    case REGIMM:
         return TYPE_B;


    case SPECIAL:
    case OP_ADDI:
    case OP_ADDIU:
    case OP_SLTI:
        return TYPE_FP;


    default:
        cerr<<"UNKNOWN TYPE"<<endl;
        return -1;

    }
}


/*----------------------------To Store MeM---------------------------*/
vector<int> MeM;


/*---------------------------To Handle Write--------------------------*/
struct WBlock
{
  int b ;
  int result ;
};

vector<WBlock> WBRecord;

/*----------------------------SIM'S helpers---------------------------*/
#define movep(x) (x==5)?0:x+1       //move pointer
#define moveb(x) (x==0)?5:x-1       //move back

#define moveRF(x) (x==9)?0:x+1
#define moveRB(x) (x==0)?9:x-1

#define moveRS(x)  (x==0)?9:x-1    // find precessor in RS
#define moveROB(x) (x==0)?5:x-1    // find precessor in ROB

/*-------------------------------SIM'S DS-----------------------------*/

queue<MInstruct>   IQ;        // The IQ,which would be exactly the same as ins_set, but it's a queue for convenience
RS   RStations[10];           // The RS, every time we need a loop to find the empty slot
ROB  ROBuffer[6];             // The ROB, use a circular buffer for in-order commit
RF   RegisterStat[32];        // The Register File
BTB  BTBuffer[16];            // The BTB, we will use LRU

int Regs[32];                 // The registers' values,note: destination is "8" means Regs[7]!!

struct node                   // The chain of LRU algorithm
{
  int index;
  node* next;
};

node* LRUChain = NULL;

node* ListInit()
{
    node* L,*M;
    L = new node();
    L->index = 0;
    L->next = NULL;
    if(L == NULL)
    {
        cerr << "fail" << endl;
    }

    M = L;
    for(int i = 1; i!= 16; ++i)
    {
        M->next = new node();
        M->next->index = i;
        M->next->next  = NULL;
        M = M->next;
    }
    return L;
}



/*---------------------------Copy For Print----------------------------*/
queue<MInstruct>     IQ2;

/*---------------------------Copy For Delay----------------------------*/

RS   RStations2[10];           // The RS, every time we need a loop to find the empty slot
ROB  ROBuffer2[6];             // The ROB, use a circular buffer for in-order commit
RF   RegisterStat2[32];        // The Register File
BTB  BTBuffer2[16];            // The BTB, we will use LRU
int Regs2[32];                 // The registers' values,note: destination is "8" means Regs[7]!!


/*----------------------------helpers----------------------------------*/
int RS_num = 0;
int RS_head = 0;
int RS_tail = -1;
int ROB_head = 0;
int ROB_tail = -1;
int ROB_num  = 0;


int lookup(int addr_look)       // return the index of found BTB entry, -1 for none
{
    for(int i = 0; i!= 16; ++i)
    {
        if(BTBuffer[i].PC == addr_look)
            return i;
    }
    return -1;
}


int findEmptySlot()
{
    for(int i = 0; i!= 16; ++i)
    {
        if (BTBuffer2[i].PC == 0 )  // unused slot
        {
          return i;
        }
    }
    return -1;
}
int LRU(BTB & tmp)   // insert a given BTB entyr into BTB, if it hits, just replace, else use LRU to kick sb out, sit in tail, return 1 if success, else -1
{
    int index = lookup(tmp.PC);

    bool full = true;
    if(index == -1)  // not hit
    {
        //LRU, if not full, just find out one empty slot, else replace
        // first find the index LRU,then replace;
     int x = findEmptySlot();
     if(x == -1) full = true;
     else
     {
         BTBuffer2[x] = tmp;
         LRUTail(x);
         full  = false;
     }

        if(full)        // full buffer, need to replace
        {
            // use a function to get the head of chain, and place it to tail after updating
            int b = LRUChain->index;
            BTBuffer2[b] = tmp;
            LRUTail(b);
        }
        return 1;
    }
    else             //hit, update and put it to the tail
    {
        BTBuffer2[index] = tmp; // modify BTBuffer2, cause it will be written into BTBuffer at end of cycle
        LRUTail(index);

        return 1;
    }
    return -1;// unknown errors
}

void LRUTail(int i)            // put the slot with index i to the tail, this linked list must be 16 length
{
    if(i < 0 || i > 15)
    {
        cerr << "Try to find in LRU with index: " << i << endl;
        exit(-1);
    }
    node * p = LRUChain;
    node * q = p->next;

    if(p->index == i)           // we should move the head
    {
        LRUChain = q;
        while(q->next !=NULL)
        {
            q = q->next;
        }
        // q now is at the end
        q->next = p;            // head become the tail
        p->next = NULL;
        return;
    }

    while(q != NULL && q->index != i)
    {
        p = p->next;
        q = q->next;
    }
    // now q point to the one with index, p is the prior
    if(q->next == NULL) // put tail to tail is meanless
    {
        return;
    }
    else
    {
        p->next = q->next;
        while(p->next != NULL)
        {
            p = p->next;
        }
        p->next = q;
        q->next  =NULL;
    }
    return;
}

int RS_insert(RS tmp)           // return the location
{
#if 0
    if(RS_num == 0)
    {
        RStations2[0]  =tmp;
        RS_num++;
        return 0;
    }
    for(int i = 0; i != 10; ++i)
    {
        if(!RStations[i].busy )  // check original but change "2"
        {
            RS fuck = RStations[moveRS(i)];
            if(fuck.busy)
            {
            RStations2[i] = tmp;// by this we erase the old record
            RS_num++;
            return i;           // stop when we find at least one empty slot
            }
            else continue;
        }
    }
    return -1;
#endif
    if(RS_num == 10)//full buffer
    {
      cerr<< "Try to insert into a full RS!!" <<endl;
      return -1;
    }
    else //Not full
    {
        RS_tail = moveRF(RS_tail);
        // save at tail
        RStations2[RS_tail] = tmp;
        RS_num++;
        return RS_tail;
    }
}
// Do not need RS_delete since we could just set "busy" to false ,but make sure we need to make RS_num-1.
void RS_delete(int i)
{
# if 0
    RStations2[i].busy = false;
    RS_num--;
#endif

    // at commit stage, need to fix the head
    if(RS_num == 0)//full buffer
      {
        cerr<< "Try to delete from an empty RS!!" <<endl;
        return;
      }
#if 1
    else if(RS_head == RS_tail)    // delete the only element
    {
        RS_head = 0;
        RS_tail = -1;

    }
#endif
    else
    {
        // clear head slot
        RS_head = moveRF(RS_head);
        RS_num--;
    }
}


int ROB_insert(ROB tmp) //return location
{
  if(ROB_num == 6)//full buffer
  {
    cerr<< "Try to insert into a full ROB!!" <<endl;
    return -1;
  }
  else //Not full
  {
      ROB_tail = movep(ROB_tail);
      // save at tail
      ROBuffer2[ROB_tail] = tmp;
      ROB_num++;
      return ROB_tail;
  }
}

void ROB_delete()
{
    // at commit stage, need to fix the head
    if(ROB_num == 0)//full buffer
      {
        cerr<< "Try to delete from an empty ROB!!" <<endl;
        return;
      }
#if 1
    else if(ROB_head == ROB_tail)    // delete the only element
    {
        ROB_head = 0;
        ROB_tail = -1;

    }
#endif
    else
    {
        // clear head slot
        ROB_head = movep(ROB_head);
        ROB_num--;
    }
}


/* No specific data structure for Memory.
 * The Memory is data after address 716
 * Need to modify and read single slot of bin file
 * /

/*--------------------------------PRINTING-----------------------------*/
void printAll()
{
 int i;

 cout<< "*******************************RS**************************************" <<endl;
 for (i = 0; i!= 10;++i)
 {
     RStations[i].print();
     if(i == RS_head) cout << "<--head" << endl;
 }
 cout<< "*******************************ROB*************************************" <<endl;
 // in printting ROB, we use head to tail
 for (i = 0; i!= 6;++i)
 {
     ROBuffer[i].print();
     if(i == ROB_head) cout << "<--head" << endl;
     else if(i == ROB_tail) cout << "<--tail" << endl;
     else cout << endl;
 }


 cout<< "*******************************RG**************************************" <<endl;
 for (i = 0; i!= 32;++i)
 {
     RegisterStat[i].print();
 }
 #if  1
 cout<< "*******************************BTB*************************************" <<endl;
 for (i = 0; i!= 16;++i)
 {
     BTBuffer[i].print();
 }
 cout<< "*******************************REGS*************************************" <<endl;
 for (i = 0; i!= 32;++i)
 {
     cout<<Regs[i]<<endl;
 }
 cout<< "*******************************MEM**************************************" <<endl;
 cout<< "716:   ";
 for(int x = 0; x!= MeM.size();  ++x)
 {
     cout << MeM[x] << "   ";
 }
cout<<endl;
cout<< "********************************LRU**************************************" <<endl;
node * p = LRUChain;

int y = 0;
while(p != NULL )//  && y != 16)
{
    cout << p->index<< " " ;
    p = p->next;
    y++;
}
cout << endl;

#endif
return;
}


/*-------------------------------------Dis Part--------------------------------------*/

/*-------------------------------GLOBAL VARS---------------------------*/
int buffer;                 // An int is 32-bits and can store exactly one instruction
int inc_buffer[32];         // Each slot represents a specific bit
vector<MInstruct> ins_set;  // A vector to store all the disassembled instructions


/*----------------------------ASSISTANT FUNCT---------------------------*/
void code_trans()       // help to transfer the int to 32slots array
{
    for(int i = 0; i!= 8; ++i)
    {
        inc_buffer[i] = ((buffer>>(7-i))&1);
    }
    for(int i = 8; i!= 16; ++i)
    {
        inc_buffer[i] = ((buffer>>(23-i))&1);
    }
    for(int i = 16; i!= 24; ++i)
    {
        inc_buffer[i] = ((buffer>>(39-i))&1);
    }
    for(int i = 24; i!= 32; ++i)
    {
        inc_buffer[i] = ((buffer>>(55-i))&1);
    }

    return;
}

int get_Opcode()        // extract opcode(first 6 bits from buffer, i.e 0-5)
{
    int res = 0;
    for(int i = 0; i!=6; ++i)
    {
        res = res*10 + inc_buffer[i];
    }
    return res;
}

int get_rs()            // extract rs (6-10 bits from buffer), and transfer it into 10's number
{
    int res = 0;
    for(int i = 6; i!=11; ++i)
    {
        res = res*2 + inc_buffer[i];
    }
    return res;
}

int get_rt()            // extract rt (11-15 bits from buffer), and transfer it into 10's number
{
    int res = 0;
    for(int i = 11; i!=16; ++i)
    {
        res = res*2 + inc_buffer[i];
    }
    return res;
}

int get_rd()            // extract rs (16-20 bits from buffer), and transfer it into 10's number
{
    int res = 0;
    for(int i = 16; i!=21; ++i)
    {
        res = res*2 + inc_buffer[i];
    }
    return res;
}

int get_shamt()         // extract shamt (21-25 bits from buffer), and transfer it into 10's number
{
    int res = 0;
    for(int i = 21; i != 26; ++i)
    {
        res = res*2 + inc_buffer[i];
    }
    return res;
}

int get_funct()         // extract rs (26-31 bits from buffer), and transfer it into 10's number
{
    int res = 0;
    for(int i = 26; i!=32; ++i)
    {
        res = res*10 + inc_buffer[i];
    }
    return res;
}

int get_immediate()     // extract immediate (16-31 bits from buffer), and transfer it into 10's number
{
    int res = 0;
    for(int i = 16; i!=32; ++i)
    {
        res = res*2 + inc_buffer[i];
    }
    return (res > 32767)? res-65536:res; //Range is +32767~-32768, 1st bit is sign
}

long long get_offset()  // extract offset (6-31 bits from buffer), and transfer it into 10's number
{
    long long res = 0;
    for(int i = 6; i!=32; ++i)
    {
        res = res*2 + inc_buffer[i];
    }
    return (res > 33554431)? res-67108864:res; //Range is +33554431~-33554432, 1st bit is sign
}

/* The function for disassembler, i.e "dis" option*/
void disassembler(ifstream& infile, ofstream& outfile) //index is the  given index of that in input command arguments,i.e IO files
{
    int MemTmp = 0;
    int curr_addr = 600; // stratr location of codes
    bool flag = false;  // To indicate whether we meet break
/*
    std::ifstream infile("fibonacci_bin.bin",std::ios::binary); //Open the file
    if(!infile)
    {
        cerr<< "open error!" <<endl;
        exit(-1);
    }
*/

    while(infile.read((char *)&buffer,4))   // Each time read 4bytes,i.e 32bits
    {
        code_trans();   // Now codes in inc_buffer

        if(!flag)   // Haven't met BREAK
        {
            for(int i = 0; i!= 32; ++i) // Print the 32bits ,and address
            {
                if(i == 6 || i == 11 || i == 16 || i == 21 || i == 26)
                    //outfile << " " ;
                ;//outfile<< inc_buffer[i];
            }
            //outfile << " " << curr_addr << " " ;

            switch(get_Opcode())
            {
            case OP_SW:  //p280
                ins_set.push_back(MInstruct(OP_SW,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "SW R" << get_rt() << ", " << get_immediate() << "(R" << get_rs() << ")" << endl;
            break;


            case OP_LW:   //p171
                ins_set.push_back(MInstruct(OP_LW,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "LW R" << get_rt() << ", " << get_immediate() << "(R" << get_rs() << ")" << endl;
            break;

            case OP_J :
                ins_set.push_back(MInstruct(OP_J, -1,-1,-1,-1,-1,get_offset()));
                //outfile << "J #" << get_offset() * 4 << endl;  // Times by 4 because we shift 2 bits
            break;

            case OP_BEQ:
                ins_set.push_back(MInstruct(OP_BEQ,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "BEQ R" << get_rs() << ", R" << get_rt() << ", #" << get_immediate()*4 << endl;// Timed by 4
            break;

            case OP_BNE:    //Unchecked
                ins_set.push_back(MInstruct(OP_BNE,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "BNE R" << get_rs() << ", R" << get_rt() << ", #" << get_immediate()*4 << endl;// Timed by 4
            break;

            case REGIMM:    //Unckecked
                if(1 == get_rt())   //BGEZ
                {
                    ins_set.push_back(MInstruct(REGIMM,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    //outfile << "BGEZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else if(0 == get_rt())                //BLTZ
                {
                    ins_set.push_back(MInstruct(REGIMM,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    //outfile << "BLTZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else
                {
                    cerr << "Unknown Instruction Type : REGIMM WITH "<< get_rt() << endl;
                    exit(-1);
                }
            break;


            case OP_BGTZ:   //Unckecked
                if(0 == get_rt())   //BGEZ
                {
                    ins_set.push_back(MInstruct(OP_BGTZ,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    //outfile << "BGTZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else
                {
                    cerr << "Unknown Instruction Type : BGTZ WITH "  << get_rt() << endl;
                    exit(-1);
                }
            break;

            case OP_BLEZ:   //Unckecked
                if(0 == get_rt())   //BGEZ
                {
                    ins_set.push_back(MInstruct(OP_BLEZ,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    //outfile << "BLEZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else
                {
                    cerr << "Unknown Instruction Type : BLEZ WITH "  << get_rt() << endl;
                    exit(-1);
                }
            break;

            case OP_ADDI:   /* Need to check neg or pos*/
                ins_set.push_back(MInstruct(OP_ADDI,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "ADDI R" << get_rt() << ", R" << get_rs() << ", #" << get_immediate() << endl;
            break;

            case OP_ADDIU:  /* Unckecked*/
                ins_set.push_back(MInstruct(OP_ADDIU,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "ADDIU R" << get_rt() << ", R" << get_rs() << ", #" << get_immediate() << endl;
            break;

            case OP_SLTI:   /* Unchecked*/
                ins_set.push_back(MInstruct(OP_SLTI,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                //outfile << "SLTI R" << get_rt() << ", R" << get_rs() << ", #" << get_immediate() << endl;
            break;

            /*SPECIAL CODE NEED TO CHECK FUNCT BITS*/
            case SPECIAL:
                switch(get_funct())
                {
                    case FC_BREAK:
                        ins_set.push_back(MInstruct(SPECIAL,-1,-1,-1,-1,FC_BREAK,-1));//for break we don't throw in value now
                        //outfile << "BREAK" << endl;
                        flag = true;
                    break;

                    case FC_SLT:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SLT WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SLT,-1));
                        //outfile << "SLT R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_SLTU:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SLTU WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SLTU,-1));
                        //outfile << "SLTU R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_SLL:
                        if(get_rs() != 0)
                        {
                            cerr<< "SLL WITH RS UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        else if(get_rt() == 0 && get_rd() == 0 && get_shamt() == 0)      /*check whether it's NOP*/
                        {
                            ins_set.push_back(MInstruct(SPECIAL,0,0,0,0,0,-1));
                            //outfile << "NOP" << endl;
                        }
                        else
                        {
                            ins_set.push_back(MInstruct(SPECIAL,0,get_rt(),get_rd(),get_shamt(),FC_SLL,-1));
                            //outfile << "SLL R" << get_rd() << ", R" << get_rt() << ", #" << get_shamt() << endl;
                        }
                    break;

                    case FC_SRL:
                    if(get_rs() != 0)
                    {
                        cerr<< "SRL WITH RS UNEQUAL TO 0" <<endl;
                        exit(-1);
                    }
                    //outfile << "SRL R" << get_rd() << ", R" << get_rt() << ", #" << get_shamt() << endl;
                    break;

                    case FC_SRA:
                        if(get_rs() != 0)
                        {
                            cerr<< "SRA WITH RS UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,0,get_rt(),get_rd(),get_shamt(),FC_SRA,-1));
                        //outfile << "SRA R" << get_rd() << ", R" << get_rt() << ", #" << get_shamt() << endl;
                    break;

                    case FC_SUB:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SUB WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SUB,-1));
                        //outfile << "SUB R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_SUBU:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SUBU WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SUBU,-1));
                        //outfile << "SUBU R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_ADD:
                        if(get_shamt() != 0)
                        {
                            cerr<< "ADD WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_ADD,-1));
                        //outfile << "ADD R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_ADDU:
                        if(get_shamt() != 0)
                        {
                            cerr<< "ADDU WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_ADDU,-1));
                        //outfile << "ADDU R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_AND:
                        if(get_shamt() != 0)
                        {
                            cerr<< "ADD WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_AND,-1));
                        //outfile << "AND R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_OR:
                        if(get_shamt() != 0)
                        {
                            cerr<< "OR WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_OR,-1));
                        //outfile << "OR R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_XOR:
                        if(get_shamt() != 0)
                        {
                            cerr<< "XOR WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_XOR,-1));
                        //outfile << "XOR R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_NOR:
                        if(get_shamt() != 0)
                        {
                            cerr<< "NOR WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_NOR,-1));
                        //outfile << "NOR R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                default:
                    cerr<< "UNKNOWN FUNCT" << get_funct() << "AT ADDRESS " << curr_addr <<endl;
                    exit(-1);
                }
                break;


            default:
                cerr<< "UNKNOWN opcode" << get_funct() << "AT ADDRESS " << curr_addr <<endl;
                exit(-1);

           }
           curr_addr += 4;
        }

        else    // Printing the 32 bits plus address and 2's complement number
        {
            line = curr_addr;
            for(int i = 0; i!= 32; ++i) // Print the 32bits ,and address
            {
                //outfile<< inc_buffer[i];
            }
            //outfile << " " << curr_addr ;
            curr_addr += 4;

            //Transfer to original code from 2's complement
            if(inc_buffer[0] == 0)  //pos
            {
                long long res = 0;
                for(int i = 1; i!=32; ++i)
                {
                    res = res*2 + inc_buffer[i];
                }
                //outfile << " " << res <<endl;
                MemTmp = res;
            }
            else                    //neg
            {
                long long res = 0;
                //first flip, then +1, except the highest bit,
                for(int i = 1; i!=32; ++i)
                {
                    inc_buffer[i] = (inc_buffer[i] == 0)? 1:0;
                }
                for(int i = 31; i!= 0;--i)
                {
                    if(inc_buffer[i] == 0)
                    {
                        inc_buffer[i] = 1;
                        break;
                    }
                    else
                    {
                        inc_buffer[i] = 0;
                    }
                }
                // now the original code

                for(int i = 1; i!=32; ++i)
                {
                    res = res*2 + inc_buffer[i];
                }
                //outfile << " " <<  (0 - res) <<endl;
                MemTmp = 0-res;
            }
            // If addr is bigger than 716, then add the value to MeM
            if(curr_addr > 716)
            {
                MeM.push_back(MemTmp);
            }

        }
    }
    infile.close(); // close here, may be not safe enough
    //outfile.close();
    return ;
}

void disassembler2(ifstream& infile, ofstream& outfile) //index is the  given index of that in input command arguments,i.e IO files
{
    int MemTmp = 0;
    int curr_addr = 600; // stratr location of codes
    bool flag = false;  // To indicate whether we meet break
/*
    std::ifstream infile("fibonacci_bin.bin",std::ios::binary); //Open the file
    if(!infile)
    {
        cerr<< "open error!" <<endl;
        exit(-1);
    }
*/

    while(infile.read((char *)&buffer,4))   // Each time read 4bytes,i.e 32bits
    {
        code_trans();   // Now codes in inc_buffer

        if(!flag)   // Haven't met BREAK
        {
            for(int i = 0; i!= 32; ++i) // Print the 32bits ,and address
            {
                if(i == 6 || i == 11 || i == 16 || i == 21 || i == 26)
                    outfile << " " ;
                ;outfile<< inc_buffer[i];
            }
            outfile << " " << curr_addr << " " ;

            switch(get_Opcode())
            {
            case OP_SW:  //p280
                ins_set.push_back(MInstruct(OP_SW,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "SW R" << get_rt() << ", " << get_immediate() << "(R" << get_rs() << ")" << endl;
            break;


            case OP_LW:   //p171
                ins_set.push_back(MInstruct(OP_LW,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "LW R" << get_rt() << ", " << get_immediate() << "(R" << get_rs() << ")" << endl;
            break;

            case OP_J :
                ins_set.push_back(MInstruct(OP_J, -1,-1,-1,-1,-1,get_offset()));
                outfile << "J #" << get_offset() * 4 << endl;  // Times by 4 because we shift 2 bits
            break;

            case OP_BEQ:
                ins_set.push_back(MInstruct(OP_BEQ,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "BEQ R" << get_rs() << ", R" << get_rt() << ", #" << get_immediate()*4 << endl;// Timed by 4
            break;

            case OP_BNE:    //Unchecked
                ins_set.push_back(MInstruct(OP_BNE,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "BNE R" << get_rs() << ", R" << get_rt() << ", #" << get_immediate()*4 << endl;// Timed by 4
            break;

            case REGIMM:    //Unckecked
                if(1 == get_rt())   //BGEZ
                {
                    ins_set.push_back(MInstruct(REGIMM,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    outfile << "BGEZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else if(0 == get_rt())                //BLTZ
                {
                    ins_set.push_back(MInstruct(REGIMM,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    outfile << "BLTZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else
                {
                    cerr << "Unknown Instruction Type : REGIMM WITH "<< get_rt() << endl;
                    exit(-1);
                }
            break;


            case OP_BGTZ:   //Unckecked
                if(0 == get_rt())   //BGEZ
                {
                    ins_set.push_back(MInstruct(OP_BGTZ,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    outfile << "BGTZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else
                {
                    cerr << "Unknown Instruction Type : BGTZ WITH "  << get_rt() << endl;
                    exit(-1);
                }
            break;

            case OP_BLEZ:   //Unckecked
                if(0 == get_rt())   //BGEZ
                {
                    ins_set.push_back(MInstruct(OP_BLEZ,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                    outfile << "BLEZ R" << get_rs() << ", #" << get_immediate()*4 << endl;                // Timed by 4
                }
                else
                {
                    cerr << "Unknown Instruction Type : BLEZ WITH "  << get_rt() << endl;
                    exit(-1);
                }
            break;

            case OP_ADDI:   /* Need to check neg or pos*/
                ins_set.push_back(MInstruct(OP_ADDI,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "ADDI R" << get_rt() << ", R" << get_rs() << ", #" << get_immediate() << endl;
            break;

            case OP_ADDIU:  /* Unckecked*/
                ins_set.push_back(MInstruct(OP_ADDIU,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "ADDIU R" << get_rt() << ", R" << get_rs() << ", #" << get_immediate() << endl;
            break;

            case OP_SLTI:   /* Unchecked*/
                ins_set.push_back(MInstruct(OP_SLTI,get_rs(),get_rt(),-1,-1,-1,get_immediate()));
                outfile << "SLTI R" << get_rt() << ", R" << get_rs() << ", #" << get_immediate() << endl;
            break;

            /*SPECIAL CODE NEED TO CHECK FUNCT BITS*/
            case SPECIAL:
                switch(get_funct())
                {
                    case FC_BREAK:
                        ins_set.push_back(MInstruct(SPECIAL,-1,-1,-1,-1,FC_BREAK,-1));//for break we don't throw in value now
                        outfile << "BREAK" << endl;
                        flag = true;
                    break;

                    case FC_SLT:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SLT WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SLT,-1));
                        outfile << "SLT R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_SLTU:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SLTU WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SLTU,-1));
                        outfile << "SLTU R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_SLL:
                        if(get_rs() != 0)
                        {
                            cerr<< "SLL WITH RS UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        else if(get_rt() == 0 && get_rd() == 0 && get_shamt() == 0)      /*check whether it's NOP*/
                        {
                            ins_set.push_back(MInstruct(SPECIAL,0,0,0,0,0,-1));
                            outfile << "NOP" << endl;
                        }
                        else
                        {
                            ins_set.push_back(MInstruct(SPECIAL,0,get_rt(),get_rd(),get_shamt(),FC_SLL,-1));
                            outfile << "SLL R" << get_rd() << ", R" << get_rt() << ", #" << get_shamt() << endl;
                        }
                    break;

                    case FC_SRL:
                    if(get_rs() != 0)
                    {
                        cerr<< "SRL WITH RS UNEQUAL TO 0" <<endl;
                        exit(-1);
                    }
                    outfile << "SRL R" << get_rd() << ", R" << get_rt() << ", #" << get_shamt() << endl;
                    break;

                    case FC_SRA:
                        if(get_rs() != 0)
                        {
                            cerr<< "SRA WITH RS UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,0,get_rt(),get_rd(),get_shamt(),FC_SRA,-1));
                        outfile << "SRA R" << get_rd() << ", R" << get_rt() << ", #" << get_shamt() << endl;
                    break;

                    case FC_SUB:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SUB WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SUB,-1));
                        outfile << "SUB R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_SUBU:
                        if(get_shamt() != 0)
                        {
                            cerr<< "SUBU WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_SUBU,-1));
                        outfile << "SUBU R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_ADD:
                        if(get_shamt() != 0)
                        {
                            cerr<< "ADD WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_ADD,-1));
                        outfile << "ADD R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_ADDU:
                        if(get_shamt() != 0)
                        {
                            cerr<< "ADDU WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_ADDU,-1));
                        outfile << "ADDU R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_AND:
                        if(get_shamt() != 0)
                        {
                            cerr<< "ADD WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_AND,-1));
                        outfile << "AND R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_OR:
                        if(get_shamt() != 0)
                        {
                            cerr<< "OR WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_OR,-1));
                        outfile << "OR R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_XOR:
                        if(get_shamt() != 0)
                        {
                            cerr<< "XOR WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_XOR,-1));
                        outfile << "XOR R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                    case FC_NOR:
                        if(get_shamt() != 0)
                        {
                            cerr<< "NOR WITH SHAMT UNEQUAL TO 0" <<endl;
                            exit(-1);
                        }
                        ins_set.push_back(MInstruct(SPECIAL,get_rs(),get_rt(),get_rd(),0,FC_NOR,-1));
                        outfile << "NOR R" << get_rd() << ", R" << get_rs() << ", R" << get_rt() << endl;
                    break;

                default:
                    cerr<< "UNKNOWN FUNCT" << get_funct() << "AT ADDRESS " << curr_addr <<endl;
                    exit(-1);
                }
                break;


            default:
                cerr<< "UNKNOWN opcode" << get_funct() << "AT ADDRESS " << curr_addr <<endl;
                exit(-1);

           }
           curr_addr += 4;
        }

        else    // Printing the 32 bits plus address and 2's complement number
        {
            line = curr_addr;
            for(int i = 0; i!= 32; ++i) // Print the 32bits ,and address
            {
                outfile<< inc_buffer[i];
            }
            outfile << " " << curr_addr ;
            curr_addr += 4;

            //Transfer to original code from 2's complement
            if(inc_buffer[0] == 0)  //pos
            {
                long long res = 0;
                for(int i = 1; i!=32; ++i)
                {
                    res = res*2 + inc_buffer[i];
                }
                outfile << " " << res <<endl;
                MemTmp = res;
            }
            else                    //neg
            {
                long long res = 0;
                //first flip, then +1, except the highest bit,
                for(int i = 1; i!=32; ++i)
                {
                    inc_buffer[i] = (inc_buffer[i] == 0)? 1:0;
                }
                for(int i = 31; i!= 0;--i)
                {
                    if(inc_buffer[i] == 0)
                    {
                        inc_buffer[i] = 1;
                        break;
                    }
                    else
                    {
                        inc_buffer[i] = 0;
                    }
                }
                // now the original code

                for(int i = 1; i!=32; ++i)
                {
                    res = res*2 + inc_buffer[i];
                }
                outfile << " " <<  (0 - res) <<endl;
                MemTmp = 0-res;
            }
            // If addr is bigger than 716, then add the value to MeM
            if(curr_addr > 716)
            {
                MeM.push_back(MemTmp);
            }

        }
    }
    infile.close(); // close here, may be not safe enough
    outfile.close();
    return ;
}


void InstructPrinter(int i,ofstream& outfile) // print given instruction in ins_set with index i
{
    if(ins_set.size() == 0)
    {
        return;
    }

    if(i >= ins_set.size())
    {
        cerr<< "RANGE EPAND!"    ;
    }
    MInstruct tmp = ins_set[i];
    switch(tmp.Opcode)
    {
    case OP_SW:  //p280
        outfile << "SW R" << tmp.rt << ", " << tmp.immediate << "(R" << tmp.rs << ")" ;
        break;

    case OP_LW:   //p171
        outfile << "LW R" << tmp.rt << ", " << tmp.immediate << "(R" << tmp.rs << ")" ;
        break;

    case OP_J :
        outfile << "J #" << tmp.immediate * 4 ;  // Times by 4 because we shift 2 bits
        break;

    case OP_BEQ:
        outfile << "BEQ R" << tmp.rs << ", R" << tmp.rt << ", #" << tmp.immediate*4 ;// Timed by 4
        break;

     case OP_BNE:    //Unchecked
        outfile << "BNE R" << tmp.rs << ", R" << tmp.rt << ", #" << tmp.immediate*4 ;// Timed by 4
        break;

     case REGIMM:    //Unckecked
         if(1 == tmp.rt)   //BGEZ
         {
             outfile << "BGEZ R" << tmp.rs << ", #" << tmp.immediate*4 ;                // Timed by 4
         }
         else if(0 == get_rt())                //BLTZ
         {
             outfile << "BLTZ R" << tmp.rs << ", #" << tmp.immediate*4 ;                // Timed by 4
         }
         else
         {
             cerr << "Unknown Instruction Type : REGIMM WITH "<< tmp.rt    ;
              exit(-1);
         }
         break;

     case OP_BGTZ:   //Unckecked
        if(0 == tmp.rt)   //BGEZ
        {
            outfile << "BGTZ R" << tmp.rs << ", #" << tmp.immediate*4 ;                // Timed by 4
        }
         else
        {
            outfile << "Unknown Instruction Type : BGTZ WITH "  << tmp.rt ;
            exit(-1);
        }
        break;

     case OP_BLEZ:   //Unckecked
       if(0 == tmp.rt)   //BGEZ
       {
           outfile << "BLEZ R" << tmp.rs << ", #" << tmp.immediate*4 ;                // Timed by
       }
       else
       {
            cerr << "Unknown Instruction Type : BLEZ WITH "  << tmp.rt ;
            exit(-1);
       }
       break;

     case OP_ADDI:   /* Need to check neg or pos*/
        outfile << "ADDI R" << tmp.rt << ", R" << tmp.rs << ", #" << tmp.immediate ;
        break;

     case OP_ADDIU:  /* Unckecked*/
        outfile << "ADDIU R" << tmp.rt << ", R" << tmp.rs << ", #" << tmp.immediate ;
        break;

     case OP_SLTI:   /* Unchecked*/
        outfile << "SLTI R" << tmp.rt << ", R" << tmp.rs << ", #" << tmp.immediate ;
        break;

            /*SPECIAL CODE NEED TO CHECK FUNCT BITS*/
            case SPECIAL:
                switch(tmp.funct)
                {
                    case FC_BREAK:
                        outfile << "BREAK" ;
//                        flag = true;
                    break;

                    case FC_SLT:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "SLT WITH SHAMT UNEQUAL TO 0" ;
                            exit(-1);
                        }
                        outfile << "SLT R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt ;
                    break;

                    case FC_SLTU:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "SLTU WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "SLTU R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_SLL:
                        if(tmp.rs != 0)
                        {
                            cerr<< "SLL WITH RS UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        else if(tmp.rt == 0 && tmp.rd == 0 && tmp.shamt == 0)      /*check whether it's NOP*/
                        {
                            outfile << "NOP"    ;
                        }
                        else
                        {
                            outfile << "SLL R" << tmp.rd << ", R" << tmp.rt << ", #" << tmp.shamt    ;
                        }
                    break;

                    case FC_SRL:
                    if(tmp.rs != 0)
                    {
                        cerr<< "SRL WITH RS UNEQUAL TO 0"   ;
                        exit(-1);
                    }
                    outfile << "SRL R" << tmp.rd << ", R" << tmp.rt << ", #" << tmp.shamt    ;
                    break;

                    case FC_SRA:
                        if(tmp.rs != 0)
                        {
                            cerr<< "SRA WITH RS UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "SRA R" << tmp.rd << ", R" << tmp.rt << ", #" << tmp.shamt    ;
                    break;

                    case FC_SUB:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "SUB WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "SUB R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_SUBU:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "SUBU WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "SUBU R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_ADD:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "ADD WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "ADD R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_ADDU:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "ADDU WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "ADDU R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_AND:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "ADD WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "AND R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_OR:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "OR WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "OR R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_XOR:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "XOR WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "XOR R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                    case FC_NOR:
                        if(tmp.shamt != 0)
                        {
                            cerr<< "NOR WITH SHAMT UNEQUAL TO 0"   ;
                            exit(-1);
                        }
                        outfile << "NOR R" << tmp.rd << ", R" << tmp.rs << ", R" << tmp.rt    ;
                    break;

                default:
                    cerr<< "UNKNOWN FUNCT" << tmp.funct   ;
                    exit(-1);
                }
                break;


            default:
                cerr<< "UNKNOWN opcode" << tmp.funct << "AT ADDRESS "   ;
                exit(-1);

           }
    return ;
}

/*-------------------------------------Sim Print--------------------------------------*/
void printCycle(ofstream& outfile)
{
    int cnt = 0;
    int cntR = 0;
    outfile << "IQ:"<< endl;
    MInstruct tmpI;
    while(IQ2.size() != 0)  // clear IQ2
    {
        IQ2.pop();
    }
    //1. print IQ
    while(IQ.size() != 0)
    {
        tmpI = IQ.front();
        outfile<< "[";
        InstructPrinter(tmpI.index,outfile);
        outfile<<"]"<< endl;
        IQ.pop();
        IQ2.push(tmpI);
    }
    while(IQ2.size() != 0)
    {
        tmpI = IQ2.front();
        IQ2.pop();
        IQ.push(tmpI);
    }


    //2. print RS
#if 1
    outfile << "RS:"<< endl;
    int RS_cnt = 0;
    int print_head = 0;
    // find head & tail
    for(int i = 0; i!= 10; ++i)
    {
       if(RStations[i].busy)   // an occupied slot
       {
           RS_cnt++;
       }
    }
    if(RS_cnt == 10)
    {
    for(int i = 0; i!= 10; ++i) // RStations size is 10
    {
        if(RStations[i].busy)   // an occupied slot
        {
            outfile<< "[";
            InstructPrinter(ROBuffer[RStations[i].dest].instruction,outfile);
            outfile<<"]"<< endl;
        }
    }
    }
    for(int i = 0; i!= 10; ++i)
    {
        if(RStations[i].busy && !RStations[moveRB(i)].busy)
        {
            print_head = i;
            break;
        }
    }
    while(RStations[print_head].busy)
    {
        outfile<< "[";
        InstructPrinter(ROBuffer[RStations[print_head].dest].instruction,outfile);
        outfile<<"]"<< endl;
        print_head = moveRF(print_head);
    }
#endif
    //3. print ROB(from head to tail)
    outfile << "ROB:"<< endl;
    // empty ROB
    for(int a  = 0; a != 6; ++a)
    {
        if(ROBuffer[a].busy) cnt ++;
    }
    if(cnt == 0) ;
    // single element
    else if(ROB_head == ROB_tail)
    {
        outfile<< "[";
        InstructPrinter(ROBuffer[ROB_head].instruction,outfile);
        outfile<<"]"<< endl;
    }
    else
    {
        int tmp_for_print_ROB = ROB_head;
        while(tmp_for_print_ROB != ROB_tail)
        {
            outfile<< "[";
            InstructPrinter(ROBuffer[tmp_for_print_ROB].instruction,outfile);
            outfile<<"]"<< endl;
            tmp_for_print_ROB = movep(tmp_for_print_ROB);
        }
        // at this point, tmp_for_ROB is ROB_tail
        outfile<< "[";
        InstructPrinter(ROBuffer[tmp_for_print_ROB].instruction,outfile);
        outfile<<"]"<< endl;
    }


    //4. print BTB
    outfile << "BTB:"<< endl;
    for(int i = 0; i!= 16; ++i) // RStations size is 10
    {
        if(BTBuffer[i].PC != 0)   // an occupied slot
        {
            outfile << "[Entry "<< i+1 <<"]" << "<" << BTBuffer[i].PC<<","<<BTBuffer[i].real<<","<<BTBuffer[i].predictor <<">"<< endl;
        }
    }
    //5. print Registers
    outfile<< "Registers:" << endl;
    outfile << "R00:    " ;
    for(int i = 0; i!=8 ; ++i)
    {
        outfile << Regs[i] << "	" ;
    }
    outfile << endl;

    outfile << "R08:	" ;
    for(int i = 8; i!=16 ; ++i)
    {
        outfile << Regs[i] << "	" ;
    }
    outfile << endl;

    outfile << "R16:    ";
    for(int i = 16; i!=24 ; ++i)
    {
        outfile << Regs[i] << "	" ;
    }
    outfile << endl;

    outfile << "R24:    " ;
    for(int i = 24; i!=32 ; ++i)
    {
        outfile << Regs[i] << "	" ;
    }
    outfile << endl;

    // 6. print MeM
    outfile<< "Data Segment:" << endl;
    outfile<< "716:    ";
    for(int x = 0; x!= MeM.size();  ++x)
    {
        outfile << MeM[x] << "	";
    }
    outfile<<endl;
    return;
}


/*-------------------------------------Sim Part--------------------------------------*/
// At every stage, we read from original, but write into "2"
int num_of_inst = 0;    // to record which inst it is, could be used as the index of ins_set

inline int pc_trans(int pc)
{
    return (pc-600)/4;
}

void IFStage()          // this is the last stage executed in each cycle, so it would change the PC to decide which instruction picked next clock
{
    int a = pc_trans(PC);
    if(a >= ins_set.size()) return;    // don't pick any more instruction
    if(flag_whether_ROB_flush) return; // don't pick and update PC cause PC is given by mispredicted jump
    IQ.push(ins_set[pc_trans(PC)]);  // fetch the instruction
   // cout << "IQ size is "<< IQ.size() << endl;
    int slot = lookup(PC);
    if(slot == -1)                   // not in BTB
    {
        PC += 4;                     // PC = PC+4 for normal instructions and untaken J
    }
    else if(BTBuffer[slot].predictor == 1)// Hit and it's predicted as taken
    {
        PC = BTBuffer[slot].target;
        IQ.back().taken = true;      // Record the prediction
        LRUTail(slot);
    }
    else                             // Predicted as untaken
    {
        LRUTail(slot);
        PC += 4;
    }
    return;
}

void ISStage()
{
    //cout << "IQ size before ISStage is " << IQ.size() << endl;
    if(PC > ins_set.size()*4 +600) return;
    //cout << " we did issue " << endl;
    int a_for_test = 0;
    int b_for_test = 0;
    int type = 0;
    int r,b;
    RS tmp1;
    ROB tmp2;
    if(IQ.size() == 0)    // now instruction in IQ
    {
       // cout << "Empty IQ!" <<endl;
        return;
    }

    for(int i = 0; i!=12; ++i)
    {
        if(RStations[i].busy)
            a_for_test++;
    }
    for(int i = 0; i!=6; ++i)
    {
        if(ROBuffer[i].busy)
            b_for_test++;
    }
    RS_num = a_for_test;
    ROB_num = b_for_test;
  //  cout << "RS_num is "<<RS_num <<" ROB_num is "<< ROB_num << endl;
    if(RS_num != 10 && ROB_num != 6)        //both avaliable
    {
        MInstruct tmp = IQ.front();
        IQ.pop();                           //extract from IQ's head
//        cout << "We did pop and IQ size now is "<< IQ.size() <<endl;
        // 3 different types
        type = type_trans(tmp.Opcode);
       // cout<< "Type is : " << type<<endl;
        if(TYPE_FP == type)
        {
        switch(tmp.Opcode)
        {
        case OP_SLTI:
        case OP_ADDIU:
        case OP_ADDI:   // for this type, rs is rs, rt is the imm, and rd is rt
        //    cout<< tmp.rs << " " << tmp.rt << " " << tmp.rd << " " << tmp.immediate << endl;
            tmp1 = RS();
            tmp2 = ROB();
            // First ocupy slots, using r&b to record location, note: we insert into "2"
            r = RS_insert(tmp1);
            b = ROB_insert(tmp2);
        //    cout << r << " "<< b << endl;
            // Then set value
            if(RegisterStat[tmp.rs].busy)
            {
                int h = RegisterStat[tmp.rs].reorder;
                if(ROBuffer[h].ready)
                {
                    RStations2[r].Vj = ROBuffer[h].value;
                    RStations2[r].Qj = INT_MAX;
                }
                else
                {
                    RStations2[r].Qj = h;
                }
            }
            else
            {
                RStations2[r].Vj = Regs[tmp.rs];
                RStations2[r].Qj = INT_MAX;
            }

            RStations2[r].Vk = tmp.immediate; //Imm
            RStations2[r].Qk = INT_MAX;


            RStations2[r].busy = true;
            RStations2[r].dest = b;
            RStations2[r].Opcode = tmp.Opcode;

            RegisterStat2[tmp.rt].reorder = b;
            RegisterStat2[tmp.rt].busy = true;

            ROBuffer2[b].instruction = tmp.index;
            ROBuffer2[b].destination = tmp.rt; //destination is actually rt!!
            ROBuffer2[b].busy = true;
            ROBuffer2[b].ready = false;
            ROBuffer2[b].state = ISSUE;
            break;

        case SPECIAL:
            switch(tmp.funct)
            {

            case FC_BREAK:
                tmp1 = RS();
                tmp2 = ROB();
                r = RS_insert(tmp1);
                b = ROB_insert(tmp2);   // Now 2 new slots in "2" are occupied

                RStations2[r].Qj = INT_MAX;
                RStations2[r].Qk = INT_MAX;

                //RS entry
                RStations2[r].busy = false;
                RStations2[r].dest = b;
                RStations2[r].Opcode = tmp.Opcode;
                RStations2[r].done = true;
                //ROB entry
                ROBuffer2[b].instruction = tmp.index;
                ROBuffer2[b].busy = true;
                ROBuffer2[b].ready = true;
                ROBuffer2[b].state = WRITE;
                ROBuffer2[b].done = true;
                break;


            case FC_SLL:
                if(tmp.rs == 0 && tmp.rt == 0 && tmp.rd ==0 && tmp.shamt == 0 )   // this is NOP
                {
                    tmp1 = RS();
                    tmp2 = ROB();
                    r = RS_insert(tmp1);
                    b = ROB_insert(tmp2);   // Now 2 new slots in "2" are occupied

                    RStations2[r].Qj = INT_MAX;
                    RStations2[r].Qk = INT_MAX;

                    //RS entry
                    RStations2[r].busy = false; // by Final project, we don't put this into RS
                    RStations2[r].dest = b;
                    RStations2[r].Opcode = tmp.Opcode;
                    RStations2[r].done = true;
                    //ROB entry
                    ROBuffer2[b].instruction = tmp.index;
                    ROBuffer2[b].busy = true;
                    ROBuffer2[b].ready= true;  // NOP could be directly commited after Issue stage
                    ROBuffer2[b].state= WRITE;
                    ROBuffer2[b].done = true;
                    break;
                }// if not NOP, do as SRL .i.e
            case FC_SRA:
            case FC_SRL:        //for this type, vj is rt, vk is shmt,rd is rd.
  //          cout<< tmp.rs << " " << tmp.rt << " " << tmp.rd << " " << tmp.shamt << endl;
            tmp1 = RS();
            tmp2 = ROB();
            // First ocupy slots, using r&b to record location, note: we insert into "2"
            r = RS_insert(tmp1);
            b = ROB_insert(tmp2);

            // Then set value
            if(RegisterStat[tmp.rt].busy)
            {
               int h = RegisterStat[tmp.rt].reorder;
               if(ROBuffer[h].ready)
               {
                   RStations2[r].Vj = ROBuffer[h].value;
                   RStations2[r].Qj = INT_MAX;
               }
               else
               {
                   RStations2[r].Qj = h;
               }
            }
            else
            {
                RStations2[r].Vj = Regs[tmp.rt];
                RStations2[r].Qj = INT_MAX;
            }

            RStations2[r].Vk = tmp.shamt; //shamt
            RStations2[r].Qk = INT_MAX;

            //cout << "we did set busy to true " << endl;
            RStations2[r].busy = true;
            RStations2[r].dest = b;
            RStations2[r].Opcode = tmp.Opcode;

            RegisterStat2[tmp.rd].reorder = b;
            RegisterStat2[tmp.rd].busy = true;

            ROBuffer2[b].instruction = tmp.index;
            ROBuffer2[b].destination = tmp.rd; //destination is actually rd!!
            ROBuffer2[b].busy = true;
            ROBuffer2[b].ready = false;
            ROBuffer2[b].state = ISSUE;
            break;

            case FC_NOR:         // for this type, rs is rs, rt is the rt, and rd is rd
            case FC_XOR:
            case FC_OR:
            case FC_ADDU:
            case FC_SUBU:
            case FC_SUB:
            case FC_SLT:
            case FC_ADD:
                tmp1 = RS();
                tmp2 = ROB();
                // First ocupy slots, using r&b to record location, note: we insert into "2"
                r = RS_insert(tmp1);
                b = ROB_insert(tmp2);

                // Then set value
                if(RegisterStat[tmp.rs].busy)
                {
                    int h = RegisterStat[tmp.rs].reorder;
                    if(ROBuffer[h].ready)
                    {
                        RStations2[r].Vj = ROBuffer[h].value;
                        RStations2[r].Qj = INT_MAX;
                    }
                    else
                    {
                        RStations2[r].Qj = h;
                    }
                }
                else
            {
                RStations2[r].Vj = Regs[tmp.rs];
                RStations2[r].Qj = INT_MAX;
            }

                if(RegisterStat[tmp.rt].busy)
            {
                int h = RegisterStat[tmp.rt].reorder;
                if(ROBuffer[h].ready)
                {
                    RStations2[r].Vk = ROBuffer[h].value;
                    RStations2[r].Qk = INT_MAX;
                }
                else
                {
                    RStations2[r].Qk = h;
                }
            }
                else
            {
                RStations2[r].Vk = Regs[tmp.rt];
                RStations2[r].Qk = INT_MAX;
            }


                RStations2[r].busy = true;
                RStations2[r].dest = b;
                RStations2[r].Opcode = tmp.Opcode;

                RegisterStat2[tmp.rd].reorder = b;
                RegisterStat2[tmp.rd].busy = true;

                ROBuffer2[b].instruction = tmp.index;
                ROBuffer2[b].destination = tmp.rd; //destination is actually rd!!
                ROBuffer2[b].busy = true;
                ROBuffer2[b].ready = false;
                ROBuffer2[b].state = ISSUE;
                break;

            default:
                    cerr<< "Unknown Function Type at Issue Stage"<< tmp.funct << endl;
                    exit(-1);
                    return;
            }
            break;

        default:
            cerr<< "Unknown Operation Type"<< tmp.Opcode << endl;
            exit(-1);
            return;
        }
        }
        else if(TYPE_L == type)        // for now we only consider "LW"
        {
            //cout << "We are trying to issue a Load" << endl;
            //cout<< tmp.Opcode << "  "<< tmp.rs << " " << tmp.rt << " " << tmp.rd << " " << tmp.immediate << endl;
            // First, try to occupy slots
            tmp1 = RS();
            tmp2 = ROB();
            r = RS_insert(tmp1);
            b = ROB_insert(tmp2);   // Now 2 new slots in "2" are occupied
            // Setting Values:
            RStations2[r].busy = true;
            RStations2[r].Opcode = tmp.Opcode;
            if(RegisterStat[tmp.rs].busy)
            {
                int h = RegisterStat[tmp.rs].reorder;
                if(ROBuffer[h].ready)
                {
                    RStations2[r].Vj = ROBuffer[h].value;
                    RStations2[r].Qj = INT_MAX;
                }
                else
                {
                                                    //cout << "We are here and h is "<< h << endl;
                    RStations2[r].Qj = h;
                }
            }
            else
            {
                RStations2[r].Vj = Regs[tmp.rs];
                RStations2[r].Qj = INT_MAX;
            }
//           RStations2[r].Vk = tmp.immediate; //the offset, but for load we only check Qj
//           RStations2[r].Qk = INT_MAX;
            RStations2[r].A     = tmp.immediate;
            RStations2[r].dest  = b;   // ROB entry
            RStations2[r].done  =false;

            ROBuffer2[b].instruction = tmp.index;
            ROBuffer2[b].destination = tmp.rt; //destination is actually rt!!
            ROBuffer2[b].busy = true;
            ROBuffer2[b].ready = false;
            ROBuffer2[b].state = ISSUE;

            RegisterStat2[tmp.rt].reorder = b;
            RegisterStat2[tmp.rt].busy = true;


//                cout <<  "RStations2[r].Qj = " <<    RStations2[r].Qj << endl;
        }
        else if(TYPE_S == type)        // for now we only consider "SW"
        {
            tmp1 = RS();
            tmp2 = ROB();
            r = RS_insert(tmp1);
            b = ROB_insert(tmp2);      // Now 2 new slots in "2" are occupied
            // set values
            RStations2[r].busy = true;
            RStations2[r].Opcode = tmp.Opcode;  // rt ->(rs +imm)
            // set Vj:rs, cause we need it to perform AC in EXE stage
            if(RegisterStat[tmp.rs].busy)
            {
                int h = RegisterStat[tmp.rs].reorder;
                if(ROBuffer[h].ready)
                {
                    RStations2[r].Vj = ROBuffer[h].value;
                    RStations2[r].Qj = INT_MAX;
                }
                else
                {
                    RStations2[r].Qj = h;
                }
            }
            else
            {
                RStations2[r].Vj = Regs[tmp.rs];
                RStations2[r].Qj = INT_MAX;
            }
            // set Vk:rt, which is actually the value we want to store in
            if(RegisterStat[tmp.rt].busy)
            {
                int h = RegisterStat[tmp.rt].reorder;
                if(ROBuffer[h].ready)
                {
                    RStations2[r].Vk = ROBuffer[h].value;
                    RStations2[r].Qk = INT_MAX;
                }
                else
                {
                    RStations2[r].Qk = h;
                }
            }
                else
            {
                RStations2[r].Vk = Regs[tmp.rt];
                RStations2[r].Qk = INT_MAX;
            }

            RStations2[r].A     = tmp.immediate;
            RStations2[r].dest  = b;   // ROB entry
            RStations2[r].done  =false;

            ROBuffer2[b].instruction = tmp.index;
//            ROBuffer2[b].destination = tmp.rt; we don't have destination
            ROBuffer2[b].busy = true;
            ROBuffer2[b].ready = false;
            ROBuffer2[b].state = ISSUE;
//            we don't occupy Regs
//            RegisterStat2[tmp.rt].reorder = b;
//            RegisterStat2[tmp.rt].busy = true;

        }
        else if(TYPE_B == type)
        {
            if(tmp.Opcode == OP_BEQ || tmp.Opcode == OP_BNE)
            {
            //do sth for B_TYPE
            tmp1 = RS();
            tmp2 = ROB();
            // First ocupy slots, using r&b to record location, note: we insert into "2"
            r = RS_insert(tmp1);
            b = ROB_insert(tmp2);

            // Then set value
            // rs
            if(RegisterStat[tmp.rs].busy)
            {
                int h = RegisterStat[tmp.rs].reorder;
                if(ROBuffer[h].ready)
                {
                    RStations2[r].Vj = ROBuffer[h].value;
                    RStations2[r].Qj = INT_MAX;
                }
                else
                {
                    RStations2[r].Qj = h;
                }
            }
            else
            {
                RStations2[r].Vj = Regs[tmp.rs];
                RStations2[r].Qj = INT_MAX;
            }
            //rt
            if(RegisterStat[tmp.rt].busy)
            {
            int h = RegisterStat[tmp.rt].reorder;
            if(ROBuffer[h].ready)
            {
                RStations2[r].Vk = ROBuffer[h].value;
                RStations2[r].Qk = INT_MAX;
            }
            else
                {
                    RStations2[r].Qk = h;
                }
            }
            else
            {
                RStations2[r].Vk = Regs[tmp.rt];
                RStations2[r].Qk = INT_MAX;
            }

            //RS entry
            RStations2[r].busy = true;
            RStations2[r].dest = b;
            RStations2[r].Opcode = tmp.Opcode;
            RStations2[r].A = tmp.immediate;
            RStations2[r].result = 0;
            RStations2[r].done = false;
            //ROB entry
            ROBuffer2[b].instruction = tmp.index;
            ROBuffer2[b].busy = true;
            ROBuffer2[b].ready = false;
            ROBuffer2[b].state = ISSUE;
            ROBuffer2[b].A = tmp.immediate;
            ROBuffer2[b].done = false;
            ROBuffer2[b].oldpc = 600 + 4 *(tmp.index);  //old pc is the addr of the branch itself
//            cout << "oldpc is : " << ROBuffer2[b].oldpc << endl;
            /* WARNING:MAY BE DANGEROUS CAUSE WE USE ROB'S VALUE TO RECORD PREDICTION*/
            ROBuffer2[b].value = (tmp.taken)? 1:0;
            }
            else
            {
                //do sth for BGEZ .i.e
                tmp1 = RS();
                tmp2 = ROB();
                // First ocupy slots, using r&b to record location, note: we insert into "2"
                r = RS_insert(tmp1);
                b = ROB_insert(tmp2);

                // Then set value
                // rs
                if(RegisterStat[tmp.rs].busy)
                {
                    int h = RegisterStat[tmp.rs].reorder;
                    if(ROBuffer[h].ready)
                    {
                        RStations2[r].Vj = ROBuffer[h].value;
                        RStations2[r].Qj = INT_MAX;
                    }
                    else
                    {
                        RStations2[r].Qj = h;
                    }
                }
                else
                {
                    RStations2[r].Vj = Regs[tmp.rs];
                    RStations2[r].Qj = INT_MAX;
                }
                //rt
                RStations2[r].Vk = 0;
                RStations2[r].Qk = INT_MAX;


                //RS entry
                RStations2[r].busy = true;
                RStations2[r].dest = b;
                RStations2[r].Opcode = tmp.Opcode;
                RStations2[r].A = tmp.immediate;
                RStations2[r].result = 0;
                RStations2[r].done = false;
                //ROB entry
                ROBuffer2[b].instruction = tmp.index;
                ROBuffer2[b].busy = true;
                ROBuffer2[b].ready = false;
                ROBuffer2[b].state = ISSUE;
                ROBuffer2[b].A = tmp.immediate;
                ROBuffer2[b].done = false;
                ROBuffer2[b].oldpc = 600 + 4 *(tmp.index);  //old pc is the addr of the branch itself
    //            cout << "oldpc is : " << ROBuffer2[b].oldpc << endl;
                /* WARNING:MAY BE DANGEROUS CAUSE WE USE ROB'S VALUE TO RECORD PREDICTION*/
                ROBuffer2[b].value = (tmp.taken)? 1:0;
            }

        }
        else if(TYPE_J == type)
        {
            tmp1 = RS();
            tmp2 = ROB();
            r = RS_insert(tmp1);
            b = ROB_insert(tmp2);   // Now 2 new slots in "2" are occupied

            RStations2[r].Qj = INT_MAX;
            RStations2[r].Qk = INT_MAX;

            //RS entry
            RStations2[r].busy = true;
            RStations2[r].dest = b;
            RStations2[r].Opcode = tmp.Opcode;
            RStations2[r].A = tmp.immediate*4;
            RStations2[r].result = 0;
            RStations2[r].done = false;
            //ROB entry
            ROBuffer2[b].instruction = tmp.index;
            ROBuffer2[b].busy = true;
            ROBuffer2[b].ready = false;
            ROBuffer2[b].state = ISSUE;
            ROBuffer2[b].A = tmp.immediate*4;
            ROBuffer2[b].done = false;
            ROBuffer2[b].oldpc = 600 + 4 *(tmp.index);  //old pc is the addr of the branch itself
//            cout << "oldpc is : " << ROBuffer2[b].oldpc << endl;
            /* WARNING:MAY BE DANGEROUS CAUSE WE USE ROB'S VALUE TO RECORD PREDICTION*/
            ROBuffer2[b].value = (tmp.taken)? 1:0;

        }
        else
        {
            cerr<<"UNKOWN OP TYPE:"<< type <<endl;

            exit(-1);
        }
    }
    //else we do nothing , just stall

//    printAll();   // For testing
    return;
}

void EXEStage()
{
    bool flag1 = false;      // these are 2 flags to incicate whether SW's some operands ready at this cycle
    bool flag2 = false;
    //cout << "IQ size before EXE is" << IQ.size() << endl;
    for(int i = 0; i!= 10; ++i) //search all slots
    {
        bool flag = false;
        int  l = -1;
//        int relative = -1;   // to record the RS corresponding to given ROB
        if(RStations[i].busy && ROBuffer[RStations[i].dest].state == EXEL1) // a valid slot and it finished LOAD1
        {
           if(type_trans(RStations[i].Opcode) != TYPE_L)
           {
               cerr << " An non-Load instuction has Load1 state!!"<< endl;
               exit(-1);
           }
           else if(!RStations[i].done)
           {
               cerr << " A Load instuction after Load1 state with done being false!"<< endl;
               exit(-1);
           }
           else // do things
           {
               flag = true;
               l = RStations[i].dest;   //ROB entry
               if( ROB_head == l)
               {
                //  cout << "The Load in step2 become the head" << endl;
                   flag = true;
               }
               else                        // check whether other prior L/S finished AC
               {
                   while(l != ROB_head)
                   {
                       l = moveb(l);
#if 0
                       cout << "l is "<< l;
                       cout     << "Optype is "<< type_trans(ins_set[ROBuffer[l].instruction].Opcode);
                       cout     <<  "Addr is"   << ROBuffer[l].A;
                       cout     <<  "State is" << ROBuffer[l].state;
                       cout     <<  endl;
#endif
                       if(type_trans(ins_set[ROBuffer[l].instruction].Opcode)== TYPE_S && ROBuffer[l].A == RStations[i].A && ROBuffer[l].state != COMMIT)    // is S with same address
                       {
                           flag = false;
                           //cout << "We did make flag false" << endl;
                           break;
                       }
                   }
               }
               if(flag)
               {
                   // step2 for load
                   int index = addr_trans(RStations[i].A);
//                   ROBuffer2[RStations[i].dest].ready = true;
//                   ROBuffer2[RStations[i].dest].value = MeM[index];
                   // only read the MeMory, it's not ready, and after this clock, the result is unavaliable since it need one more cycle for broadcasting
                   RStations2[i].result = MeM[index/4];
                   ROBuffer2[RStations[i].dest].state = EXEL2;
               }
           }
        }
        if(RStations[i].busy && ROBuffer[RStations[i].dest].state == ISSUE)   // a valid slot and it finished IF Stage
        {
        switch( type_trans(RStations[i].Opcode) )
        {
        case TYPE_FP:
            if(RStations[i].Qj == INT_MAX && RStations[i].Qk == INT_MAX)
            {
                switch(RStations[i].Opcode) // do calculation depends on opcode
                {
                    case OP_SLTI:
                    RStations2[i].result = (RStations[i].Vj < RStations[i].Vk)? 1:0;
                    RStations2[i].done   = true;
                    ROBuffer2[RStations[i].dest].state = EXECUTE;
                    break;

                    case OP_ADDIU:
                    case OP_ADDI:
                    RStations2[i].result = RStations[i].Vj + RStations[i].Vk;
                    RStations2[i].done   = true;
                    ROBuffer2[RStations[i].dest].state = EXECUTE;
                    break;

                    case SPECIAL:
                                 //           cout <<"fuck" << endl;
                    switch(ins_set[ROBuffer[RStations[i].dest].instruction].funct)
                    {
                        case FC_BREAK:
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                        case FC_SLL:

                            RStations2[i].result = RStations[i].Vj << RStations[i].Vk;
                            RStations2[i].done   = true;
                            ROBuffer2[RStations[i].dest].state = EXECUTE;

                        break;

                        case FC_SRA:
                        case FC_SRL:
                        RStations2[i].result = RStations[i].Vj >> RStations[i].Vk;
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                    // NOR,XOR,OR are bit op
                        case FC_NOR:
                        RStations2[i].result = ~(RStations[i].Vj | RStations[i].Vk);
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                        case FC_XOR:
                        RStations2[i].result = RStations[i].Vj ^ RStations[i].Vk;
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                        case FC_OR:
//                        RStations2[i].result = ((RStations[i].Vj + RStations[i].Vk)>0)? 1:0;
                        RStations2[i].result = RStations[i].Vj | RStations[i].Vk;
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                        case FC_SUBU:
                        case FC_SUB:
                        RStations2[i].result = RStations[i].Vj - RStations[i].Vk;
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                        case FC_SLT:
                        RStations2[i].result = (RStations[i].Vj < RStations[i].Vk)? 1:0;
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;

                        case FC_ADDU:
                        case FC_ADD:
                        RStations2[i].result = RStations[i].Vj + RStations[i].Vk;
                        RStations2[i].done   = true;
                        ROBuffer2[RStations[i].dest].state = EXECUTE;
                        break;
                    default:
                        cerr<< "Unknown Function Type at EXE Stage"<< ins_set[ROBuffer[RStations[i].dest].instruction].funct <<  " ROB entry is " << RStations[i].dest<<  " RS entry is " << i <<endl;
                        exit(-1);
                        return;
                    }
                    break;

                default:
                    cerr<< "Unknown Operation Type"<< RStations[i].Opcode << endl;
                    exit(-1);
                    return;
                }
            }
            break;

        case TYPE_S:
            if(RStations[i].Qj == INT_MAX && !RStations[i].done)  // base is ready and not computed
            {
                flag = true;
                l = RStations[i].dest;   //ROB entry
                if( ROB_head == l)
                {
                    flag = true;
                }
                else                        // check whether other prior L/S finished AC
                {
                    while(l != ROB_head)
                    {
                        l = moveb(l);
                        if((type_trans(ins_set[ROBuffer[l].instruction].Opcode)== TYPE_L ||type_trans(ins_set[ROBuffer[l].instruction].Opcode)== TYPE_S) && ROBuffer[l].state<= 1)    // is L/S and is still in Issue stage
                        {
                            flag = false;
                            break;
                        }
                    }
                }
                if(flag)
                {
                    //
                    RStations2[i].A = RStations[i].A + (RStations[i].Vj)*4;
//                    RStations2[i].result = RStations2[i].A;
                    RStations2[i].done = true;
                    ROBuffer2[RStations[i].dest].A = RStations2[i].A;
         //           ROBuffer2[RStations[i].dest].state = EXECUTE; don't change, sothat next cycle we can compute  Qk
                    flag1 = true;
                }
            }

            if(RStations[i].Qk == INT_MAX && !ROBuffer[RStations[i].dest].done)    // rt is ready and not computed before
            {
//                cout << " mother fucker"<< endl;
                RStations2[i].result = RStations[i].Vk;
                ROBuffer2[RStations[i].dest].value   = RStations[i].Vk;
                ROBuffer2[RStations[i].dest].done   = true;
                flag2 = true;
                //ROBuffer2[RStations[i].dest].ready   = true;
//                ROBuffer2[RStations[i].dest].state = WRITE;
            }

        if(   flag1&&flag2                  // do this 2 in same celce
           || RStations[i].done && flag2    // A already computed, we get rt in this cycle
           || ROBuffer[RStations[i].dest].done && flag1 // oopposite
           )
        {
            ROBuffer2[RStations[i].dest].ready   = true;    // ready to commit
            ROBuffer2[RStations[i].dest].state = WRITE;
        }
            break;
        case TYPE_L:
            if(RStations[i].Qj == INT_MAX)  // operands are okay
            {
                flag = true;
                l = RStations[i].dest;   //ROB entry
                //cout<< "l is "<< l << " head is" << ROB_head << endl;
                if( ROB_head == l)
                {
                    flag = true;

                }
                else                        // check whether other prior L/S finished AC
                {
                    while(l != ROB_head)
                    {
//
                        l = moveb(l);
                        if((type_trans(ins_set[ROBuffer[l].instruction].Opcode)== TYPE_L ||type_trans(ins_set[ROBuffer[l].instruction].Opcode)== TYPE_S) && ROBuffer[l].state<= 1)    // is L/S and is still in Issue stage
                        {
  //                          cout<< "l is "<< l << endl;
  //                          cout<< "State is "<<ROBuffer[l].state << endl;
                            flag = false;
                            break;
                        }
                    }
                }
                if(flag)
                {

//                    cout << "We compute the Addr for load" << endl;
                    RStations2[i].A = RStations[i].A + (RStations[i].Vj)*4;
//                    RStations2[i].result = RStations2[i].A;
                    RStations2[i].done = true;
                    ROBuffer2[RStations[i].dest].A = RStations2[i].A;
                    ROBuffer2[RStations[i].dest].state = EXEL1;
                }
                else
                {
  //                   cout << "FAIL for the Addr for load" << endl;
                }

            }
            break;

        case TYPE_B:
            if(RStations[i].Qj == INT_MAX && RStations[i].Qk == INT_MAX)    // 2 operands are okay
            {
                switch(RStations[i].Opcode)
                {
                    case REGIMM:
                    if(ins_set[ROBuffer[RStations[i].dest].instruction].rt == 1)    //BGEZ
                    {
                        if(RStations[i].Vj >= 0)  // should taken
                    {
                        RStations2[i].result = 1;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 1)//prediction is also taken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should taken but prediction is untaken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                       // RStations2[i].busy = false; // don't need RS anymore

                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }
                        else        // should not taken
                    {
                        RStations2[i].result = 0;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 0) //prediction is also untaken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);

                            //cout << "we are here" << endl;
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should not taken but prediction is taken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        //RStations2[i].busy = false; // don't need RS anymore
                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }

                    }
                    else    // BLTZ
                    {
                        if(RStations[i].Vj < 0)  // should taken
                    {
                        RStations2[i].result = 1;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 1)//prediction is also taken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should taken but prediction is untaken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                       // RStations2[i].busy = false; // don't need RS anymore

                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }
                        else        // should not taken
                    {
                        RStations2[i].result = 0;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 0) //prediction is also untaken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);

                            //cout << "we are here" << endl;
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should not taken but prediction is taken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        //RStations2[i].busy = false; // don't need RS anymore
                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }

                    }
                    break;

                    case OP_BGTZ:
                    if(RStations[i].Vj > 0)  // should taken
                {
                    RStations2[i].result = 1;
                    RStations2[i].done   = true;
                    if(ROBuffer2[RStations[i].dest].value == 1)//prediction is also taken
                    {
                        //okay
                        ROBuffer2[RStations[i].dest].correct = true;
                        //update BTB, need LRU algorithm, not yet
                        // if hit, just replace it with the new address
                        // if not inside, use LRU to replace
                        BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                        if( LRU(tmpBTB) == -1)
                        {
                            cerr << "ERROR IN LRU REPLACE" << endl;
                            exit(-1);
                        }
                    }
                    else
                    {
                        // wrong prediction, it should taken but prediction is untaken
                        ROBuffer2[RStations[i].dest].correct = false;
                        //update BTB, need LRU algorithm, not yet
                        BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                        if( LRU(tmpBTB) == -1)
                        {
                            cerr << "ERROR IN LRU REPLACE" << endl;
                            exit(-1);
                        }
                    }
                   // RStations2[i].busy = false; // don't need RS anymore

                    ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;
                    ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                    ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                }
                    else        // should not taken
                {
                    RStations2[i].result = 0;
                    RStations2[i].done   = true;
                    if(ROBuffer2[RStations[i].dest].value == 0) //prediction is also untaken
                    {
                        //okay
                        ROBuffer2[RStations[i].dest].correct = true;
                        //update BTB, need LRU algorithm, not yet
                        // if hit, just replace it with the new address
                        // if not inside, use LRU to replace
                        BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);

                        //cout << "we are here" << endl;
                        if( LRU(tmpBTB) == -1)
                        {
                            cerr << "ERROR IN LRU REPLACE" << endl;
                            exit(-1);
                        }
                    }
                    else
                    {
                        // wrong prediction, it should not taken but prediction is taken
                        ROBuffer2[RStations[i].dest].correct = false;
                        //update BTB, need LRU algorithm, not yet
                        BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                        if( LRU(tmpBTB) == -1)
                        {
                            cerr << "ERROR IN LRU REPLACE" << endl;
                            exit(-1);
                        }
                    }
                    //RStations2[i].busy = false; // don't need RS anymore
                    ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
                    ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                    ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                }
                    break;

                    case OP_BLEZ:
                    if(RStations[i].Vj <= 0)  // should taken
            {
                RStations2[i].result = 1;
                RStations2[i].done   = true;
                if(ROBuffer2[RStations[i].dest].value == 1)//prediction is also taken
                {
                    //okay
                    ROBuffer2[RStations[i].dest].correct = true;
                    //update BTB, need LRU algorithm, not yet
                    // if hit, just replace it with the new address
                    // if not inside, use LRU to replace
                    BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                    if( LRU(tmpBTB) == -1)
                    {
                        cerr << "ERROR IN LRU REPLACE" << endl;
                        exit(-1);
                    }
                }
                else
                {
                    // wrong prediction, it should taken but prediction is untaken
                    ROBuffer2[RStations[i].dest].correct = false;
                    //update BTB, need LRU algorithm, not yet
                    BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                    if( LRU(tmpBTB) == -1)
                    {
                        cerr << "ERROR IN LRU REPLACE" << endl;
                        exit(-1);
                    }
                }
               // RStations2[i].busy = false; // don't need RS anymore

                ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;
                ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
            }
                    else        // should not taken
            {
                RStations2[i].result = 0;
                RStations2[i].done   = true;
                if(ROBuffer2[RStations[i].dest].value == 0) //prediction is also untaken
                {
                    //okay
                    ROBuffer2[RStations[i].dest].correct = true;
                    //update BTB, need LRU algorithm, not yet
                    // if hit, just replace it with the new address
                    // if not inside, use LRU to replace
                    BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);

                    //cout << "we are here" << endl;
                    if( LRU(tmpBTB) == -1)
                    {
                        cerr << "ERROR IN LRU REPLACE" << endl;
                        exit(-1);
                    }
                }
                else
                {
                    // wrong prediction, it should not taken but prediction is taken
                    ROBuffer2[RStations[i].dest].correct = false;
                    //update BTB, need LRU algorithm, not yet
                    BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                    if( LRU(tmpBTB) == -1)
                    {
                        cerr << "ERROR IN LRU REPLACE" << endl;
                        exit(-1);
                    }
                }
                //RStations2[i].busy = false; // don't need RS anymore
                ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
                ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
            }
                    break;

                    case OP_BNE:
                    if(RStations[i].Vj != RStations[i].Vk)  // should taken
                    {
                        RStations2[i].result = 1;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 1)//prediction is also taken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should taken but prediction is untaken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                       // RStations2[i].busy = false; // don't need RS anymore

                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }
                    else        // should not taken
                    {
                        RStations2[i].result = 0;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 0) //prediction is also untaken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);

                            //cout << "we are here" << endl;
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should not taken but prediction is taken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        //RStations2[i].busy = false; // don't need RS anymore
                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }
                    break;

                    case OP_BEQ:
                    if(RStations[i].Vj == RStations[i].Vk)  // should taken
                    {
                        RStations2[i].result = 1;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 1)//prediction is also taken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should taken but prediction is untaken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4,1,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                       // RStations2[i].busy = false; // don't need RS anymore

                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }
                    else        // should not taken
                    {
                        RStations2[i].result = 0;
                        RStations2[i].done   = true;
                        if(ROBuffer2[RStations[i].dest].value == 0) //prediction is also untaken
                        {
                            //okay
                            ROBuffer2[RStations[i].dest].correct = true;
                            //update BTB, need LRU algorithm, not yet
                            // if hit, just replace it with the new address
                            // if not inside, use LRU to replace
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);

                            //cout << "we are here" << endl;
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        else
                        {
                            // wrong prediction, it should not taken but prediction is taken
                            ROBuffer2[RStations[i].dest].correct = false;
                            //update BTB, need LRU algorithm, not yet
                            BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].oldpc+4,0,ROBuffer[RStations[i].dest].oldpc+ROBuffer[RStations[i].dest].A*4+4);
                            if( LRU(tmpBTB) == -1)
                            {
                                cerr << "ERROR IN LRU REPLACE" << endl;
                                exit(-1);
                            }
                        }
                        //RStations2[i].busy = false; // don't need RS anymore
                        ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
                        ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
                        ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
                    }
                    break;

                default:
                    cerr<< "UKOWN JUMP TYPE AT EXE STAGE "<< RStations[i].Opcode << "ROB entry is " << RStations[i].dest<< endl;
                    exit(-1);
                    break;
                }
            }
            break;

        case TYPE_J:        // directly insert into the BTB
            if(ROBuffer2[RStations[i].dest].value == 0)     //predicted untaken
            ROBuffer2[RStations[i].dest].correct = false;
            else ROBuffer2[RStations[i].dest].correct = true;
            if(0)
            {
                BTB tmpBTB(ROBuffer[RStations[i].dest].oldpc,ROBuffer[RStations[i].dest].A,1,ROBuffer[RStations[i].dest].A);
                if( LRU(tmpBTB) == -1)
                {
                    cerr << "ERROR IN LRU REPLACE" << endl;
                exit(-1);
                }
            }
//            RStations2[i].busy = false; // don't need RS anymore
            RStations2[i].result = 1; // don't need RS anymore
//            ROBuffer2[RStations[i].dest].outcome  = RStations2[i].result;// actual outcome
            ROBuffer2[RStations[i].dest].ready  = 1;      //can be commited
            ROBuffer2[RStations[i].dest].state = WRITE;   //finish execute, set it to write so that it could statr CM next cycle
            break;

        default:
            cerr<< "UKOWN TYPE AT EXE STAGE" << endl;
            exit(-1);
            break;
        }
        }
    }
    return;
}

void WRStage()
{
    //cout << "IQ size before WR is " << IQ.size() << endl;
    for(int i = 0; i!= 10; ++i) //search all slots
    {
        int b;
        if(RStations[i].busy)   // a valid slot
        {
        if(ROBuffer[RStations[i].dest].state == EXEL2 && RStations[i].done)   // an already done operation and is after EXEL2 stage
        {
            if(type_trans(RStations[i].Opcode) != TYPE_L)
            {
                cerr << " An non-Load instuction has Load1 state!!"<< endl;
                exit(-1);
            }
            else if(!RStations[i].done)
            {
                cerr << " A Load instuction after Load2 state with done being false!"<< endl;
                exit(-1);
            }
            else    // do sth
            {
                b = RStations[i].dest;
//                RStations2[i].busy = false;

                for(int j = 0; j!= 10; ++j)
                {
                    if(RStations[j].busy) // a valid slot
                    {
                        if(RStations[j].Qj == b)
                        {
                            RStations2[j].Vj = RStations[i].result;
                            RStations2[j].Qj = INT_MAX;
                        }
                        if(RStations[j].Qk == b)
                        {
                            RStations2[j].Vk = RStations[i].result;
                            RStations2[j].Qk = INT_MAX;
                        }
                    }
                }
                WBlock WBTmp;
                WBTmp.b = b;
                WBTmp.result = RStations[i].result;
                WBRecord.push_back(WBTmp);
                ROBuffer2[b].value = RStations[i].result;
                ROBuffer2[b].ready = true;
                ROBuffer2[b].state = WRITE;

            }
        }

        if(ROBuffer[RStations[i].dest].state == EXECUTE && RStations[i].done)   // an already done operation and is after EXE stage
        {
           //NO:  we mark save as ready right after EXE stage
   #if 0
            if(type_trans(RStations[i].Opcode) == TYPE_S)                              // is store operation
          {
              b = RStations[i].dest;
              // Just modify the ROB's Value, mark it as ready
              if(RStations[i].Qk == INT_MAX)    // rt is ready
              {
//                  RStations2[i].busy = false;
                  RStations2[i].result = RStations[i].Vk;
                  ROBuffer2[b].value   = RStations[i].Vk;
                  ROBuffer2[b].ready   = true;
                  ROBuffer2[b].state = WRITE;
              }
              // else we do nothing
          }
#endif
 //         else
 //         {
            b = RStations[i].dest;
//            RStations2[i].busy = false;    // maybe too early?

              for(int j = 0; j!= 10; ++j)
            {
              if(RStations[j].busy) // a valid slot
              {
                  if(RStations[j].Qj == b)
                  {
                      RStations2[j].Vj = RStations[i].result;
                      RStations2[j].Qj = INT_MAX;
                  }
                  if(RStations[j].Qk == b)
                  {
                      RStations2[j].Vk = RStations[i].result;
                      RStations2[j].Qk = INT_MAX;
                  }
              }
   //       }
              WBlock WBTmp;
              WBTmp.b = b;
              WBTmp.result = RStations[i].result;
              WBRecord.push_back(WBTmp);

              ROBuffer2[b].value = RStations[i].result;
              ROBuffer2[b].ready = true;
              ROBuffer2[b].state = WRITE;
          }
        }
        }
    }

}

void CMStage()      // only L/S & FP operations for now
{
    //cout << "IQ size before CM is " << IQ.size() << endl;
    int h = ROB_head;
    int d;
    if(ROBuffer[h].ready)
    {
        for(int x =0; x!= 10; ++x)  // free RS
        {
            if(RStations[x].dest == h)
            {
                RStations2[x].busy = false;
            }
        }

        if(type_trans(ins_set[ROBuffer[h].instruction].Opcode) == TYPE_S)
        {
          //  cout << "slot is "<< addr_trans(ROBuffer[h].A)/4<< " and value is "<< ROBuffer[h].value << endl;
            MeM[addr_trans(ROBuffer[h].A)/4] = ROBuffer[h].value;
            //clear the ROB entry
            ROBuffer2[h].busy = false;
            ROBuffer2[h].ready= false;
            ROBuffer2[h].instruction= -1;
            ROBuffer2[h].state= COMMIT;
            ROBuffer2[h].destination= 0;
            ROBuffer2[h].value= 0;
            //ROB_delete(); do not delete here cause it will change "2", we just set the flag, and at end of each cycle, update head.and reset flag to false
            flag_whether_ROB_changed = true;
        }
        else if(type_trans(ins_set[ROBuffer[h].instruction].Opcode) == TYPE_J)
        {
            if(ROBuffer[h].correct) // predict correctly
            {
                // just let it go
                ROBuffer2[h].busy = false;
                ROBuffer2[h].ready= false;
                ROBuffer2[h].instruction= -1;
                ROBuffer2[h].state= COMMIT;
                ROBuffer2[h].destination= 0;
                ROBuffer2[h].value= 0;
                //ROB_delete();
                flag_whether_ROB_changed = true;
                //flag_whether_ROB_flush = true;      // for jump , we need always flush the ?
            }
            else    // wrong prediction
            {
                    PC = ROBuffer[h].A;
                    flag_whether_ROB_flush = true;
            }
            // inserted into BTB at this time
            BTB tmpBTB(ROBuffer[h].oldpc,ROBuffer[h].A,1,ROBuffer[h].A);
            if( LRU(tmpBTB) == -1)
            {
                cerr << "ERROR IN LRU REPLACE" << endl;
            exit(-1);
            }
        }
        else if(type_trans(ins_set[ROBuffer[h].instruction].Opcode) == TYPE_B)
        {
            if(ROBuffer[h].correct) // predict correctly
            {
                // just let it go
                ROBuffer2[h].busy = false;
                ROBuffer2[h].ready= false;
                ROBuffer2[h].instruction= -1;
                ROBuffer2[h].state= COMMIT;
                ROBuffer2[h].destination= 0;
                ROBuffer2[h].value= 0;
                //ROB_delete();
                flag_whether_ROB_changed = true;
            }
            else    // wrong prediction
            {
                if(ROBuffer[h].outcome == 1)    // predicted as untaken but actually taken
                {
                    PC = ROBuffer[h].oldpc + ROBuffer[h].A*4 + 4;    // update PC

//                    cout << "PC after update in CM stage is"<< PC<<endl;
                    flag_whether_ROB_flush = true;
                    // update BTB ,not yet
                }
                else                            // predicted as taken but actually untaken
                {
                    PC = ROBuffer[h].oldpc;
                    flag_whether_ROB_flush = true;
                    // update BTB ,not yet
                }
            }
        }
        else if(type_trans(ins_set[ROBuffer[h].instruction].Opcode) == TYPE_FP && ins_set[ROBuffer[h].instruction].Opcode == SPECIAL && ins_set[ROBuffer[h].instruction].funct == FC_SLL && ins_set[ROBuffer[h].instruction].rs == 0 && ins_set[ROBuffer[h].instruction].rt == 0 && ins_set[ROBuffer[h].instruction].rd == 0 && ins_set[ROBuffer[h].instruction].shamt == 0)
        {
            // NOP
            ROBuffer2[h].busy = false;
            ROBuffer2[h].ready= false;
            ROBuffer2[h].instruction= -1;
            ROBuffer2[h].state= COMMIT;
            ROBuffer2[h].destination= 0;
            ROBuffer2[h].value= 0;
            flag_whether_ROB_changed = true;
        }
        else if( type_trans(ins_set[ROBuffer[h].instruction].Opcode) == TYPE_FP && ins_set[ROBuffer[h].instruction].funct == FC_BREAK )
        {
            d = ROBuffer[h].destination;
            //clear the ROB entry
            ROBuffer2[h].busy = false;
            ROBuffer2[h].ready= false;
            ROBuffer2[h].instruction= -1;
            ROBuffer2[h].state= COMMIT;
            ROBuffer2[h].destination= 0;
            ROBuffer2[h].value= 0;
            //ROB_delete();
            flag_whether_ROB_changed = true;
            flag_whether_Stop        = true;
        //    cout << " we are okay here" << endl;
        }
        else
        {
            d = ROBuffer[h].destination;
            Regs2[d] = ROBuffer[h].value;
            //clear the ROB entry
            ROBuffer2[h].busy = false;
            ROBuffer2[h].ready= false;
            ROBuffer2[h].instruction= -1;
            ROBuffer2[h].state= COMMIT;
            ROBuffer2[h].destination= 0;
            ROBuffer2[h].value= 0;
            //ROB_delete();
            flag_whether_ROB_changed = true;

            if(RegisterStat[d].reorder == h)
            {
                RegisterStat2[d].busy = false;
                RegisterStat2[d].reorder =  0;
            }
        }
    }
    //else we do nothing cause the head entry of ROB is not ready,but we have to perform in-order commit
}

void printWBR()
{
    if(WBRecord.size() != 0)
    {
        cout << "*********************************WRRecord********************************" << endl;
        for(int n = 0; n!= WBRecord.size(); ++n)
        {
            cout << "b is "<<WBRecord[n].b <<" and result:  "<< WBRecord[n].result<< endl;
        }
    }
}


void simulator(ifstream& infile, ofstream& outfile)
{
    disassembler(infile, outfile);

#if 1
    //1. For every one in ins_set, set their "index" value
    for(int i = 0; i != ins_set.size();++i)
    {
        ins_set[i].index = i;
    }

    LRUChain = ListInit();

    //2.A loop , goes from IF to CM, at every stage, do things based on those given data structure

       // Infinite loop, jump out when last instruction commit
    int a = 0;
//    while(1)
    for(int a = 0; a != 100; ++a )  // done in reverse (!?)
    {
    //    cout << "IQ size at the beginning of cycle "<< a+1 << "is "  << IQ.size() << endl;
        // First, copy to "2"
        vector<WBlock>().swap(WBRecord);    // clear the record
        WBRecord.clear();

        int i = 0;
        for(i = 0; i != 10; ++i)
        {
            RStations2[i] = RStations[i];
        }
        for(i = 0; i!=6;++i)
        {
            ROBuffer2[i] = ROBuffer[i];
        }
        for(i = 0; i!= 32; ++i)
        {
            RegisterStat2[i] = RegisterStat[i];
        }
        for(i = 0; i!= 16; ++i)
        {
            BTBuffer2[i] = BTBuffer[i];
        }
        for(i = 0; i!= 32; ++i)
        {
            Regs2[i] = Regs[i];
        }

//    cout << "IQ size before Pipeline is " << IQ.size() << endl;
        CMStage();
        WRStage();
        EXEStage();
        ISStage();
        IFStage();

//        cout << "after pipeline, RStations2[3].Qj = " <<RStations2[3].Qj << endl;

        if(flag_whether_ROB_changed)
         {
            ROB_delete();
            flag_whether_ROB_changed = false;
         }

        for(i = 0; i != 10; ++i)
        {
            RStations[i] = RStations2[i];
        }
        for(i = 0; i!=6;++i)
        {
            ROBuffer[i] = ROBuffer2[i];
        }
        for(i = 0; i!= 32; ++i)
        {
            RegisterStat[i] = RegisterStat2[i];
        }
        for(i = 0; i!= 16; ++i)
        {
            BTBuffer[i] = BTBuffer2[i];
        }
        for(i = 0; i!= 32; ++i)
        {
            Regs[i] = Regs2[i];
        }

    if(WBRecord.size() != 0)
        {
            for(int n = 0; n!= WBRecord.size(); ++n)
            {

                for(int j = 0; j!= 10; ++j)
                {
                    if(RStations[j].busy) // a valid slot
                    {
                        if(RStations[j].Qj == WBRecord[n].b)
                        {
                            RStations[j].Vj = WBRecord[n].result;
                            RStations[j].Qj = INT_MAX;
                        }
                        if(RStations[j].Qk == WBRecord[n].b)
                        {
                            RStations[j].Vk = WBRecord[n].result;
                            RStations[j].Qk = INT_MAX;
                        }
                    }
                }
            }
        }
//        cout << " after CDB RStations[3].Qj = "  << RStations[3].Qj  << endl;

        if(flag_whether_ROB_flush)
        {
             ROB_head = 0;
             ROB_tail = -1;

            // clear RS, ROB, RegisterFiles,
            for(i = 0; i != 10; ++i)
            {
                RS tmp = RS();
                RStations[i] = tmp;
            }
            for(i = 0; i!=6;++i)
            {
                ROB tmp = ROB();
                ROBuffer[i] = tmp;
            }
            for(i = 0; i!= 32; ++i)
            {
                RF tmp = RF();
                RegisterStat[i] = tmp;
            }
            RS_num  = 0;
            ROB_num = 0;
            // clear IQ !!!!
            while(IQ.size() != 0)
                IQ.pop();

            flag_whether_ROB_flush = false;
    //        cout<< "we flush the DS" <<endl;
    //        cout<< "Now the PC is " << PC << endl;
        }

if( (option_m == 0 && option_n == 0)|| (a+1)>=option_m && (a+1) <= option_n )
{
        outfile<<"Cycle <"<<a+1<<">:"<<endl;
        printCycle(outfile);
}


        if(flag_whether_Stop)
        {
         //   cout << "WE MEET BREAK" << endl;
            exit(0);
        }
    }

#endif

    return;
}

/*---------------------------------------MAIN------------------------------------*/
int main(int argc, char * const argv[])
{
    //for testing
    for(int a = 0; a!= 32 ; ++a)
    {
        //Regs[a] = a%10;
        Regs[a] = 0;

    }

    if(argc < 3)
    {
        cerr << "AT LEAST 3 ARGUMENTS FOR MIPS BUT ONLY GOT " << argc << endl;
        exit(-1);
    }

    std::ifstream inputfile(argv[1],std::ios::binary); //Open the file
    if(!inputfile)
    {
        cerr<< "open input file error!" <<endl;
        exit(-1);
    }

    std::ofstream outputfile(argv[2],std::ios::binary); //Open the file
    if(!outputfile)
    {
        cerr<< "open output file error!" <<endl;
        exit(-1);
    }
#if 1
    if(argc == 3)   // no extra options
    {
        simulator(inputfile,outputfile);
    }
    else            //we got [Tm:n]
    {
        string tmp = argv[3];
        if(tmp[0] != '-')
        {
            cerr<< " UNKNOWN OPTION, -Tm:n EXPECTED "<< endl;
            exit(-1);
        }
        // extract number
        int TIndex, CIndex; // index of T and ":"
        for(int a = 0; a!= tmp.size(); ++a)
        {
            if(tmp[a] == 'T')
            {
                TIndex = a;
            }
            if(tmp[a] == ':')
            {
                CIndex = a;
            }
        }
        if(abs(TIndex-CIndex) <= 1)
        {
            cerr<< " MISSING NUMBER BETWEEN 'T' & ':'  "<< endl;
            exit(-1);
        }
        if(CIndex == tmp.size()-1)
        {
            cerr<< " MISSING N "<< endl;
            exit(-1);
        }

        for(int i = TIndex+1; i!=CIndex ; ++i)  // extract m
        {
            if(IsNum(tmp[i]))
            {
                option_m *= 10;
                option_m += (int)tmp[i]-48;
            }
        }

        for(int i = CIndex+1; i!= tmp.size() ; ++i)  // extract m
        {
            if(IsNum(tmp[i]))
            {
                option_n *= 10;
                option_n += (int)tmp[i]-48;
            }
        }

//        option_m = (int)tmp[2]-48;
//        option_n = (int)tmp[4]-48;

        if(option_m > option_n)
        {
            cerr<< "m > n !! "<< endl;
            exit(-1);
        }

//        cout << option_m<<" "<< option_n <<endl;
        simulator(inputfile,outputfile);
#if 0
        string tmp = argv[3];
        if(tmp == "dis")
        disassembler2(inputfile,outputfile);
        else if(tmp == "sim")
        simulator(inputfile,outputfile);
        else
        {
            cerr << "OPERATION FOR MIPS IS NEITHER DIS OR SIM: " << tmp << endl;
            exit(-1);
        }
#endif
    }
#endif
    //simulator(inputfile,outputfile);
}

