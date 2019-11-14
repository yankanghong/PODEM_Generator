/*
 Author: Kanghong Yan
 Class: ECE6140
 Last Date Modified: 11/12/2019
 Description: Headfile of the PODEM generator
           Path-Oriented Decision Making
 */

#ifndef __TEST_GENERATOR_HEADER__
#define __TEST_GENERATOR_HEADER__

#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <queue>
#include <set>
#include <iterator>
#include <utility>
#include <random>
#include <ratio>
#include <chrono>

#define MAX_RANDOM_ITER_NUM 500
#define PIN_FLOATING -1
#define D_POS 4  // D value = 1/0, 1 is fault-free value  
#define D_BAR 5  // Dbar = 0/1, 0 is fault-free value  

typedef std::mt19937 MT_rng;  // random number generator
typedef unsigned int uint;

typedef enum Gate_Type_ENUM {
    AND,
    OR,
    NOR,
    NAND,
    INV,
    BUF,
    XOR,
    XNOR,
    Type_Num
} Gate_Type;

struct Wire {
    long wireNum;  // -1 means PIN_FLOATING
    bool valid;    // if valid, them can be calculated
    uint value;
    bool is_pi;    // is primary input
    bool is_po;    // is primary output
};

typedef struct Node *NodePtr;
struct Node {
    Gate_Type gate;
    Wire input1;
    Wire input2;
    Wire output;
    std::vector<NodePtr> nextGates; // point to the next level gate
    std::vector<NodePtr> lastGates; // point to the last level gate, for backtrace
};

typedef struct FaultList *FaultListPtr;
typedef struct FaultList
{
    long netNum;
    uint fault_value;           // faulty value
    std::vector<NodePtr> gates; // corresponding gates for faster reference
} FaultList;

typedef struct IOPort *IOPortPtr; // primary IO port associate with gate
struct IOPort {
    long IOWireNum;
    bool valid;
    uint value;
    std::vector<NodePtr> IOGate;
};

typedef struct DF_Node* DF_NodePtr;
struct DF_Node
{
    NodePtr df_gate;
};

class PODEM_Generator {
private:
    long totalWireNum;                    // total wire number
    std::queue<FaultListPtr> gl_flist;       // global fault list
    
    std::vector<IOPortPtr> inputWires;    // primary input wires
    std::vector<IOPortPtr> outputWires;   // primary output wires 
    std::vector<NodePtr> adjList;         // store address of the circuit gate
    std::queue<std::pair<NodePtr, uint>> btQueue;   // back trace queue, nodeptr and value
    std::queue<NodePtr> calQueue;        // calculation queue

    
    
    void init_gflist(const std::string &);
    void init_circuit(const std::string &); 
    
    // utility function
    void print_input(std::ofstream &outFile);
    void print_input_assi(std::ofstream &outFile);
    friend unsigned int is_gate_xpath(NodePtr node);
    bool is_main_fault_gate(NodePtr node);
    bool is_main_fault_gate_DF(NodePtr node);
    bool mf_is_po();
    
    // function for test possible check
    bool test_possible_check(std::vector<DF_NodePtr> &D_frontier);
    bool look_ahead_check(NodePtr &);
    bool fault_in_po();
    // return net_num and value for backtrace
    void objective(long &net_num, bool &value);
    
    // Functions for Imply
    void nodeCalculation(const NodePtr &calNode);   // calculate each node
    void forward_cal();                             // forward calculation
    void imply(const uint &, const long &);      // initialize value of primary inputs
    
    // Functions for Backtrace
    unsigned int backtrace(long netNum, uint value, uint &, long & );  // return a index to inputwires, return true if there is no conflict during backtrace
    unsigned int backtrace_PO(long netNum, uint value);  // backtrace for primary output
    
    unsigned int broadcast_pi();      // broadcast primary input
    unsigned int broadcast_pi(const uint &, const long &);      // broadcast primary input
    bool assign_pi(const uint &, const long &);
    void objective(long &, uint &, std::vector<DF_NodePtr> ) ;    // get objective
    unsigned int sel_random(uint down_range, uint up_range);
    bool podem_sim(std::vector<DF_NodePtr>);     // podem_sim, return success or failure
    void cirReset();   // reset ready signal of circuit

public:
    PODEM_Generator() {}
    PODEM_Generator(const std::string &, const std::string &);  // constructor with ckt file and fl list file
    long getInputNum() {return (inputWires.size());}
    long getOutputNum() {return (outputWires.size());}
    long getGateNum() {return adjList.size();}
    void generate(const std::string &cirName);        // generate circuit Test
    void generatorReset();


};

#endif /* __TEST_GENERATOR_HEADER__ */
