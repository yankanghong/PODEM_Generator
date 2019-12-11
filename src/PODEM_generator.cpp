/*
 Author: Kanghong Yan
 Class: ECE6140
 Last Date Modified: 11/12/2019
 Description: implementation of PODEM_Generator class
 */

#include "PODEM_generator.h"
#include <iomanip>

using namespace std;

////////////////////////////////////////////////////////
//
// PODEM_Generator class
//
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////
// check whether the node is in xpath, if yes, return true
unsigned int is_gate_xpath(NodePtr node)
{
    if ((node->gate == INV) | (node->gate == BUF))
        return !(node->input1.valid);
    
    if ((node->gate == XOR) | (node->gate == XNOR))
    {
        if (node->input1.valid && node->input2.valid)
            return 0; // not both input is already determined
        else if (!node->input1.valid && !node->input1.valid)
            return 3; // both inputs are x-path
        else if (!node->input1.valid)
            return 1; // input1 is x-path
        else
            return 2; // input2 is x-path
    }
    
    switch(node->gate)
    {
        case AND:
        case NAND:
        {
            if ( (node->input1.valid && node->input1.value==0) | (node->input2.valid && node->input2.value==0) | (node->input1.valid&&node->input2.valid))
                return 0;  // return false when there is an control value
            else if (!node->input1.valid && !node->input2.valid)
                return 3; // both inputs are x-path
            else if (!node->input1.valid)
                return 1; // input1 is x-path
            else
                return 2; // input2 is x-path
            break;
        }
        case OR:
        case NOR:
        {
            if ( (node->input1.valid && node->input1.value) | (node->input2.valid && node->input2.value) | (node->input1.valid&&node->input2.valid))
                return 0;  // return false when there is an control value
            else if (!node->input1.valid && !node->input2.valid)
                return 3; // both inputs are x-path
            else if (!node->input1.valid)
                return 1; // input1 is x-path
            else
                return 2; // input2 is x-path
            break;
        }
            
        default:;
    }
    
    return 0;
}

// get a random number from given input range
unsigned int PODEM_Generator::sel_random(uint down_range, uint up_range)
{
    unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    MT_rng rng;      // random generator
    rng.seed(seed);
    std::uniform_int_distribution<int> ufo_dist(down_range,up_range); // get random number from 0-1
    unsigned int select(0);
    select = ufo_dist(rng);
    return select;
}

// check whether input node is a gate that contains main fault and it is in D_FRONTIER
bool PODEM_Generator::is_main_fault_gate_DF(NodePtr node)
{
    for (auto it:this->gl_flist.front()->gates)
    {
        if (it==node)  // is main fault gate
        {
            if (!node->input1.valid & !node->input2.valid) // both inputs are not valid yet
                return true;
            uint control_value(0);
            control_value = (node->gate==OR | node->gate==NOR)? 1:0;
            if ((node->input1.wireNum==gl_flist.front()->netNum) & !node->input1.valid & (node->input2.value!=control_value))
                return true;
            
            if ((node->input2.wireNum==gl_flist.front()->netNum) & !node->input2.valid & (node->input1.value!=control_value))
                return true;
        }
    }
    
    return false;
}

// check whether input node is a gate that contains main fault
bool PODEM_Generator::is_main_fault_gate(NodePtr node)
{
    for (auto it:this->gl_flist.front()->gates)
        if (it==node)  // is main fault gate
            return true;
    
    return false;
}

// check whether the main fault is PO fault
bool PODEM_Generator::mf_is_po()
{
    FaultListPtr fault = gl_flist.front();
    uint fault_value = fault->fault_value;
    long net_num = fault->netNum;
    uint result;
    
    for (result=0; result < outputWires.size(); result++) // check whether it is PO
    {
        if (net_num == outputWires[result]->IOWireNum)
        {
            result = backtrace_PO(net_num, !fault_value);
            //            printf ("backtrace_PO return: %d\n", result);
            return true;
        }
    }
    return false;
}

// look ahead and check whether D-frontier can be propagated to output, return 1 if success
bool PODEM_Generator::look_ahead_check(NodePtr &current_node)
{
    if ((current_node->output.valid & ((current_node->output.value!=D_BAR) | (current_node->output.value!=D_POS))))  // if the way is blocked, return 0
        return 0;
    
    if (current_node->output.is_po & ((current_node->output.valid & ((current_node->output.value==D_BAR) | (current_node->output.value==D_POS))) | !current_node->output.valid))
        return 1;
    
    for (auto it:current_node->nextGates)
        if (look_ahead_check(it))
            return 1;
    
    return 0;
}

// check whether the fault is propagate to output, return 0 if yes, return 1 if not
bool PODEM_Generator::fault_in_po()
{
    for (auto it:this->outputWires)
    {
        for (auto iit:it->IOGate)
        {
            if (iit->output.valid & (iit->output.value==D_BAR | iit->output.value==D_POS))
                return 0;
        }
    }
    return 1;
}

// check whether the test is possible, return 1 is success, 0 is failure
bool PODEM_Generator::test_possible_check(std::vector<DF_NodePtr> &D_frontier)
{
    D_frontier.clear();
    for (auto it:this->adjList) // update the D_frontier
    {
        if( !it->output.valid & !(it->input1.valid&&it->input2.valid) &
           ( (it->input1.valid & (it->input1.value==D_POS | it->input1.value==D_BAR))
            |(it->input2.valid & (it->input2.value==D_POS | it->input2.value==D_BAR))
            | is_main_fault_gate_DF(it)))
        {
            if (look_ahead_check(it))
            {
                DF_NodePtr df_ptr = new DF_Node;
                df_ptr->df_gate = it;
                D_frontier.push_back(df_ptr);
            }
        }
    }
    
    if (D_frontier.empty()) // test is not possible
        return 0;
    
    // check whether the fault has value violating
    for (auto it:this->gl_flist.front()->gates)
    {
        if (it->input1.valid && it->input1.wireNum==gl_flist.front()->netNum)
            if (gl_flist.front()->fault_value == it->input1.value)
                return false;  // main fault cannot be sensitized
        
        if (it->input2.valid && it->input2.wireNum==gl_flist.front()->netNum)
            if (gl_flist.front()->fault_value == it->input2.value)
                return false;  // main fault cannot be sensitized
    }

    return 1;  // return 1 is success
}

////////////////////////////////////////////////////////
// PODEM_Generator main functions
////////////////////////////////////////////////////////

PODEM_Generator::PODEM_Generator(const string &filePath, const string &faListPath):totalWireNum(0) {
    init_circuit(filePath);
    init_gflist(faListPath);
}

// initialize the global fault list
void PODEM_Generator::init_gflist(const std::string &flFile) {
    ifstream inFile(flFile);
    // encoding the wire number and fault type by following rule:
    // i.e. wireNumber=19, s-a-1, than the code would be 191
    // i.e. wireNumber=19, s-a-0, than the code would be 190
    // i.e. wireNumber=19, s-a-x, than the code would be 193 (3 represent "x" state)
    
    uint fault;
    if (inFile.is_open())
    {
        while (!inFile.eof())
        {
            FaultListPtr fl_ptr = new FaultList;
            inFile >> fl_ptr->netNum;
            string tmp;
            inFile >> tmp;
            if (tmp == "x")
                fault = 1;  // take x as 1
            else
                fault = stoi(tmp);
            
            fl_ptr->fault_value = fault;
            for (auto it:this->adjList)
                if (it->input1.wireNum==fl_ptr->netNum | it->input2.wireNum==fl_ptr->netNum)
                    fl_ptr->gates.push_back(it);
            
            this->gl_flist.push(fl_ptr);
        }
        inFile.close();
    }
    else
    {
        cout<< "Error reading fault list file, please check..."<<endl;
        exit(EXIT_FAILURE);
    }
}

void PODEM_Generator::init_circuit(const string &cirPath) {
    ifstream inFile(cirPath.c_str()); // compatiable with c98
    string gateInfo;
    if (inFile.is_open()) {
        inFile >> gateInfo;
        
        while (!inFile.eof()) {
            
            if (gateInfo != "INPUT" && gateInfo != "OUTPUT") {
                
                NodePtr newNode = new Node;
                newNode->input1.is_pi = false;
                newNode->input1.is_po = false;
                newNode->input2.is_pi = false;
                newNode->input2.is_po = false;
                newNode->output.is_po = false;
                newNode->output.is_pi = false;
                
                if (gateInfo == "INV") { //inverter
                    newNode->gate = INV;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    newNode->input2.wireNum = PIN_FLOATING;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    if (newNode->input1.wireNum > totalWireNum)
                        totalWireNum = newNode->input1.wireNum;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                    
                }
                else if (gateInfo == "BUF") {
                    newNode->gate = BUF;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    if (newNode->input1.wireNum > totalWireNum)
                        totalWireNum = newNode->input1.wireNum;
                    
                    newNode->input2.wireNum = PIN_FLOATING;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                else if (gateInfo == "AND") {
                    newNode->gate = AND;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    if (newNode->input1.wireNum > totalWireNum)
                        totalWireNum = newNode->input1.wireNum;
                    
                    inFile >> newNode->input2.wireNum;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    if (newNode->input2.wireNum > totalWireNum)
                        totalWireNum = newNode->input2.wireNum;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                else if (gateInfo == "OR") {
                    newNode->gate = OR;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    if (newNode->input1.wireNum > totalWireNum)
                        totalWireNum = newNode->input1.wireNum;
                    
                    inFile >> newNode->input2.wireNum;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    if (newNode->input2.wireNum > totalWireNum)
                        totalWireNum = newNode->input2.wireNum;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                else if (gateInfo == "NOR") {
                    newNode->gate = NOR;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    if (newNode->input1.wireNum > totalWireNum)
                        totalWireNum = newNode->input1.wireNum;
                    
                    inFile >> newNode->input2.wireNum;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    if (newNode->input2.wireNum > totalWireNum)
                        totalWireNum = newNode->input2.wireNum;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                else if (gateInfo == "NAND") {
                    newNode->gate = NAND;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    if (newNode->input1.wireNum > totalWireNum)
                        totalWireNum = newNode->input1.wireNum;
                    
                    inFile >> newNode->input2.wireNum;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    if (newNode->input2.wireNum > totalWireNum)
                        totalWireNum = newNode->input2.wireNum;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                else if (gateInfo == "XOR") {
                    newNode->gate = XOR;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    inFile >> newNode->input2.wireNum;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                else if (gateInfo == "XNOR") {
                    newNode->gate = XNOR;
                    inFile >> newNode->input1.wireNum;
                    newNode->input1.valid = false;
                    newNode->input1.value = 0;
                    
                    inFile >> newNode->input2.wireNum;
                    newNode->input2.valid = false;
                    newNode->input2.value = 0;
                    
                    inFile >> newNode->output.wireNum;
                    newNode->output.valid = false;
                    newNode->output.value = 0;
                }
                
                adjList.push_back(newNode);
            }
            else{
                if (gateInfo == "INPUT") {
                    long num;
                    inFile >> num;
                    while (num != -1) {
                        IOPortPtr newIO = new IOPort;
                        newIO->valid = false;
                        newIO->value = 0;
                        newIO->IOWireNum = num;
                        for (unsigned long i=0; i<adjList.size(); i++) {
                            if (adjList[i]->input1.wireNum == num || adjList[i]->input2.wireNum == num )
                                newIO->IOGate.push_back(adjList[i]);
                            if (adjList[i]->input1.wireNum == num)
                                adjList[i]->input1.is_pi = true;
                            if (adjList[i]->input2.wireNum == num)
                                adjList[i]->input2.is_pi = true;
                        }
                        inputWires.push_back(newIO);
                        inFile >> num;
                    }
                }
                else {
                    long num;
                    inFile >> num;
                    while (num != -1) {
                        IOPortPtr newIO = new IOPort;
                        newIO->valid = false;
                        newIO->value = 0;
                        newIO->IOWireNum = num;
                        if (num > totalWireNum)
                            totalWireNum = num;
                        for (unsigned long i=0; i<adjList.size(); i++) {
                            if (adjList[i]->output.wireNum == num)
                            {
                                newIO->IOGate.push_back(adjList[i]);
                                adjList[i]->output.is_po = true;
                            }
                            
                        }
                        outputWires.push_back(newIO);
                        inFile >> num;
                    }
                }
            }
            inFile >> gateInfo;
        }
        inFile.close();
    }
    else {
        cout<< "fail to open file, please check path!"<<endl;
        cout<< "exit now...."<<endl;
        exit(EXIT_FAILURE);
    }
    // build graph
    for (unsigned long i=0; i<adjList.size();i++) {
        for (unsigned long j=0;j<adjList.size();j++) {
            if (i!=j) {
                if ( adjList[i]->output.wireNum != PIN_FLOATING
                    && ( (adjList[j]->input1.wireNum == adjList[i]->output.wireNum)
                        ||   (adjList[j]->input2.wireNum == adjList[i]->output.wireNum))
                    )
                {
                    adjList[i]->nextGates.push_back(adjList[j]);   // store the next level gate to current node
                    adjList[j]->lastGates.push_back(adjList[i]);   // store the last level gate
                }
                
            }
        }
    }
}

// reset the circuit elements
void PODEM_Generator::cirReset()
{
    for (unsigned long i=0; i<adjList.size();i++)
    {
        adjList[i]->input1.valid = false;
        adjList[i]->input2.valid = false;
        adjList[i]->output.valid = false;
    }
}

// reset the whole values in generator
void PODEM_Generator::generatorReset()
{
    cirReset();
    std::queue<std::pair<NodePtr, uint>>().swap(btQueue); // clear the queue
    std::queue<NodePtr>().swap(calQueue); // clear the queue
    if (!calQueue.empty() || !btQueue.empty())
    {
        printf("Error clear the queues\n");
        exit(EXIT_FAILURE);
    }
    
    for (auto it:this->inputWires)
        it->valid = false;
    
}

// node calcuation, calculate the node's value, w/o xor & xnor support
void PODEM_Generator::nodeCalculation(const NodePtr &calNode)
{
    long fault_netNum;
    uint fault_val;
    fault_netNum = this->gl_flist.front()->netNum;
    fault_val = this->gl_flist.front()->fault_value;
    calNode->output.valid = true;
    
    
    calNode->output.value = (fault_val==1)? D_BAR: 0;
    
    
    if (calNode->output.wireNum==fault_netNum)
    {
        switch (calNode->gate)
        {
            case AND:
            {
                if ((calNode->input1.value==0) | (calNode->input2.value==0)) // control value
                    calNode->output.value = (fault_val==1)? D_BAR: 0;
                else if ((calNode->input1.value==1) && (calNode->input2.value==1)) // two are both non-control and know value
                    calNode->output.value = (fault_val==0)? D_POS: 1;
                else if (calNode->input1.value==1) // one of input is fault value
                    calNode->output.value = (calNode->input2.value==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                else                               // one of input is fault value
                    calNode->output.value = (calNode->input1.value==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                break;
            }
            case OR:
            {
                if ((calNode->input1.value==1) | (calNode->input2.value==1)) // control value
                    calNode->output.value = (fault_val==0)? D_POS: 1;
                else if ((calNode->input1.value==0) && (calNode->input2.value==0)) // two are both non-control and know value
                    calNode->output.value = (fault_val==1)? D_BAR: 0;
                else if (calNode->input1.value==0) // one of input is fault value
                    calNode->output.value = (calNode->input2.value==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                else                               // one of input is fault value
                    calNode->output.value = (calNode->input1.value==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                break;
            }
            case NOR:
            {
                if ((calNode->input1.value==1) | (calNode->input2.value==1)) // control value
                    calNode->output.value = (fault_val==1)? D_BAR: 0;
                else if ((calNode->input1.value==0) && (calNode->input2.value==0)) // two are both non-control and know value
                    calNode->output.value = (fault_val==0)? D_POS: 1;
                else if (calNode->input1.value==0) // one of input is fault value
                    calNode->output.value = ((!calNode->input2.value)==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                else                               // one of input is fault value
                    calNode->output.value = ((!calNode->input1.value)==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                break;
            }
            case NAND:
            {
                if (!calNode->input1.value | !calNode->input2.value) // control value
                    calNode->output.value = (fault_val==0)? D_POS: 1;
                else if ((calNode->input1.value==1) && (calNode->input2.value==1)) // two are both non-control and know value
                    calNode->output.value = (fault_val==1)? D_BAR: 0;
                else if (calNode->input1.value==1) // one of input is fault value
                    calNode->output.value = ((!calNode->input2.value)==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                else                               // one of input is fault value
                    calNode->output.value = ((!calNode->input1.value)==fault_val)? fault_val: (fault_val? D_BAR : D_POS);
                break;
            }
            case INV:
                calNode->output.value = ((!calNode->input1.value)==fault_val)? fault_val :(fault_val? D_BAR : D_POS);
                break;
            case BUF:
                calNode->output.value = (calNode->input1.value==fault_val)? fault_val :(fault_val? D_BAR : D_POS);
                break;
                // case XOR:
                // {
                //     if (calNode->input1.value == calNode->input2.value)
                //         calNode->output.value = 0;
                //     else if (calNode->input1.value==0)
                //         calNode->output.value = calNode->input2.value;
                //     else if (calNode->input2.value==0)
                //         calNode->output.value = calNode->input1.value;
                //     else if (calNode->input1.value==1)  // invert input2
                //         calNode->output.value = (calNode->input2.value==0)? 1 :( (calNode->input2.value==1)? 0: ((calNode->input2.value==D_POS)? D_BAR : D_POS));
                //     else if (calNode->input2.value==1) // invert input1
                //         calNode->output.value = (calNode->input1.value==0)? 1 :( (calNode->input1.value==1)? 0: ((calNode->input1.value==D_POS)? D_BAR : D_POS));
                //     else // d_bar and d
                //         calNode->output.value = 1;
                //     break;
                // }
                // case XNOR:
                // {
                //     if (calNode->input1.value == calNode->input2.value)
                //         calNode->output.value = 1;
                //     else if (calNode->input1.value==1)
                //         calNode->output.value = calNode->input2.value;
                //     else if (calNode->input2.value==1)
                //         calNode->output.value = calNode->input1.value;
                //     else if (calNode->input1.value==0)  // invert input2
                //         calNode->output.value = (calNode->input2.value==0)? 1 :( (calNode->input2.value==1)? 0: ((calNode->input2.value==D_POS)? D_BAR : D_POS));
                //     else if (calNode->input2.value==0) // invert input1
                //         calNode->output.value = (calNode->input1.value==0)? 1 :( (calNode->input1.value==1)? 0: ((calNode->input1.value==D_POS)? D_BAR : D_POS));
                //     else // d_bar and d
                //         calNode->output.value = 0;
                //     break;
                // }
            default:;
        }
    }
    else
    {
        switch (calNode->gate)
        {
            case AND:
            {
                if ((calNode->input1.value==0) | (calNode->input2.value==0)) // control value
                    calNode->output.value = 0;
                else if ((calNode->input1.value==1) && (calNode->input2.value==1)) // two are both non-control and know value
                    calNode->output.value = 1;
                else if (calNode->input1.value==1) // one of input is fault value
                    calNode->output.value = calNode->input2.value;
                else                               // one of input is fault value
                    calNode->output.value = calNode->input1.value;
                break;
            }
            case OR:
            {
                if ((calNode->input1.value==1) | (calNode->input2.value==1)) // control value
                    calNode->output.value = 1;
                else if ((calNode->input1.value==0) && (calNode->input2.value==0)) // two are both non-control and know value
                    calNode->output.value = 0;
                else if (calNode->input1.value==0) // one of input is fault value
                    calNode->output.value = calNode->input2.value;
                else                               // one of input is fault value
                    calNode->output.value = calNode->input1.value;
                break;
            }
            case NOR:
            {
                if ((calNode->input1.value==1) | (calNode->input2.value==1)) // control value
                    calNode->output.value = 0;
                else if ((calNode->input1.value==0) && (calNode->input2.value==0)) // two are both non-control and know value
                    calNode->output.value = 1;
                else if (calNode->input1.value==0) // one of input is fault value
                    calNode->output.value = (calNode->input2.value==D_POS)? D_BAR : D_POS;
                else                               // one of input is fault value
                    calNode->output.value = (calNode->input1.value==D_POS)? D_BAR : D_POS;
                break;
            }
            case NAND:
            {
                if (!calNode->input1.value | !calNode->input2.value) // control value
                    calNode->output.value = 1;
                else if ((calNode->input1.value==1) && (calNode->input2.value==1)) // two are both non-control and know value
                    calNode->output.value = 0;
                else if (calNode->input1.value==1) // one of input is fault value
                    calNode->output.value = (calNode->input2.value==D_POS)? D_BAR : D_POS;
                else                               // one of input is fault value
                    calNode->output.value = (calNode->input1.value==D_POS)? D_BAR : D_POS;
                break;
            }
            case INV:
                calNode->output.value = (calNode->input1.value==0)? 1 :( (calNode->input1.value==1)? 0: ((calNode->input1.value==D_POS)? D_BAR : D_POS));
                break;
            case BUF:
                calNode->output.value = calNode->input1.value;
                break;
            case XOR:
            {
                if (calNode->input1.value == calNode->input2.value)
                    calNode->output.value = 0;
                else if (calNode->input1.value==0)
                    calNode->output.value = calNode->input2.value;
                else if (calNode->input2.value==0)
                    calNode->output.value = calNode->input1.value;
                else if (calNode->input1.value==1)  // invert input2
                    calNode->output.value = (calNode->input2.value==0)? 1 :( (calNode->input2.value==1)? 0: ((calNode->input2.value==D_POS)? D_BAR : D_POS));
                else if (calNode->input2.value==1) // invert input1
                    calNode->output.value = (calNode->input1.value==0)? 1 :( (calNode->input1.value==1)? 0: ((calNode->input1.value==D_POS)? D_BAR : D_POS));
                else // d_bar and d
                    calNode->output.value = 1;
                break;
            }
            case XNOR:
            {
                if (calNode->input1.value == calNode->input2.value)
                    calNode->output.value = 1;
                else if (calNode->input1.value==1)
                    calNode->output.value = calNode->input2.value;
                else if (calNode->input2.value==1)
                    calNode->output.value = calNode->input1.value;
                else if (calNode->input1.value==0)  // invert input2
                    calNode->output.value = (calNode->input2.value==0)? 1 :( (calNode->input2.value==1)? 0: ((calNode->input2.value==D_POS)? D_BAR : D_POS));
                else if (calNode->input2.value==0) // invert input1
                    calNode->output.value = (calNode->input1.value==0)? 1 :( (calNode->input1.value==1)? 0: ((calNode->input1.value==D_POS)? D_BAR : D_POS));
                else // d_bar and d
                    calNode->output.value = 0;
                break;
            }        default:;
        }
    }
}

void PODEM_Generator::forward_cal()
{
    NodePtr calNode;
    while (!calQueue.empty())
    {
        calNode = calQueue.front();
        
        nodeCalculation(calNode);
        
        for (unsigned long i=0; i<calNode->nextGates.size(); i++)
        {
            if (calNode->nextGates[i]->input1.wireNum == calNode->output.wireNum)
            {
                calNode->nextGates[i]->input1.value = calNode->output.value;
                calNode->nextGates[i]->input1.valid = true;
            }
            
            if (calNode->nextGates[i]->input2.wireNum == calNode->output.wireNum)
            {
                calNode->nextGates[i]->input2.value = calNode->output.value;
                calNode->nextGates[i]->input2.valid = true;
            }
            
            // different decision with dual-input gate and single-input gate
            if (calNode->nextGates[i]->gate == INV || calNode->nextGates[i]->gate == BUF)
            {
                if (calNode->nextGates[i]->input1.valid)
                    calQueue.push(calNode->nextGates[i]);
            }
            else
            {
                if (calNode->nextGates[i]->gate == NAND || calNode->nextGates[i]->gate == AND)
                {
                    if ( (calNode->nextGates[i]->input1.valid && (calNode->nextGates[i]->input1.value==0))
                        | (calNode->nextGates[i]->input2.valid && (calNode->nextGates[i]->input2.value==0))
                        | (calNode->nextGates[i]->input1.valid && calNode->nextGates[i]->input2.valid))
                        calQueue.push(calNode->nextGates[i]);
                }
                else if (calNode->nextGates[i]->gate == NOR || calNode->nextGates[i]->gate == OR)
                {
                    if ( (calNode->nextGates[i]->input1.valid && (calNode->nextGates[i]->input1.value==1))
                        | (calNode->nextGates[i]->input2.valid && (calNode->nextGates[i]->input2.value==1))
                        | (calNode->nextGates[i]->input1.valid && calNode->nextGates[i]->input2.valid))
                        calQueue.push(calNode->nextGates[i]);
                }
            }
        }
        calQueue.pop();
    }
}

void PODEM_Generator::imply(const uint &value, const long &netNum)
{
    uint i;
    assign_pi(value, netNum);
    cirReset();
    i = broadcast_pi();
    
    if (i==1) // PI assigment conflict
    {
        printf("Error imply PI assignment conflict...\n");
        exit(EXIT_FAILURE);
    }
    
    for (unsigned long i=0; i<inputWires.size(); i++)
    {
        for (unsigned long k=0; k < inputWires[i]->IOGate.size(); k++)
        {
            // handle differently with dual-input gate and single-input gate
            if (inputWires[i]->IOGate[k]->gate == INV || inputWires[i]->IOGate[k]->gate == BUF)
            {
                if (inputWires[i]->IOGate[k]->input1.valid)
                    calQueue.push(inputWires[i]->IOGate[k]);
            }
            else
            {
                if (inputWires[i]->IOGate[k]->gate == NAND || inputWires[i]->IOGate[k]->gate == AND)
                {
                    if ( (inputWires[i]->IOGate[k]->input1.valid && (inputWires[i]->IOGate[k]->input1.value==0))
                        | (inputWires[i]->IOGate[k]->input2.valid && (inputWires[i]->IOGate[k]->input2.value==0))
                        | (inputWires[i]->IOGate[k]->input1.valid && inputWires[i]->IOGate[k]->input2.valid))
                        calQueue.push(inputWires[i]->IOGate[k]);
                }
                else if (inputWires[i]->IOGate[k]->gate == NOR || inputWires[i]->IOGate[k]->gate == OR)
                {
                    if ( (inputWires[i]->IOGate[k]->input1.valid && (inputWires[i]->IOGate[k]->input1.value==1))
                        | (inputWires[i]->IOGate[k]->input2.valid && (inputWires[i]->IOGate[k]->input2.value==1))
                        | (inputWires[i]->IOGate[k]->input1.valid && inputWires[i]->IOGate[k]->input2.valid))
                        calQueue.push(inputWires[i]->IOGate[k]);
                }
            }
        }
    }
    
    if (!calQueue.empty())
        forward_cal();
    
}


void PODEM_Generator::objective(long &net_num, uint &value, std::vector<DF_NodePtr> D_frontier)
{
    if (!D_frontier.size())
        return;
    
    for (auto it:D_frontier) // pirority for main fault to be sensitized
    {
        if (is_main_fault_gate(it->df_gate))
        {
            if ((!it->df_gate->input1.valid & (it->df_gate->input1.wireNum==this->gl_flist.front()->netNum))
                |
                (!it->df_gate->input2.valid & (it->df_gate->input2.wireNum==this->gl_flist.front()->netNum)))
            {
                net_num = this->gl_flist.front()->netNum;
                value = this->gl_flist.front()->fault_value;
                value = (value==0)? 1 :( (value==1)? 0: ((value==D_POS)? D_BAR : D_POS));
                return ;
            }
        }
    }
    
    uint i(0);
    if (D_frontier.size()!=1)
        i = sel_random(0,static_cast<uint>(D_frontier.size())-1);
    
    switch(D_frontier[i]->df_gate->gate)
    {
        case NAND:
        case AND:
        {
            if (!D_frontier[i]->df_gate->input1.valid)
                net_num = D_frontier[i]->df_gate->input1.wireNum;
            else
                net_num = D_frontier[i]->df_gate->input2.wireNum;
            value = 1;
            break;
        }
        case OR:
        case NOR:
        {
            if (!D_frontier[i]->df_gate->input1.valid)
                net_num = D_frontier[i]->df_gate->input1.wireNum;
            else
                net_num = D_frontier[i]->df_gate->input2.wireNum;
            value = 0;
            break;
        }
        case XOR:
        case XNOR:
        {
            if (!D_frontier[i]->df_gate->input1.valid)
                net_num = D_frontier[i]->df_gate->input1.wireNum;
            else
                net_num = D_frontier[i]->df_gate->input2.wireNum;
            value = sel_random(0, 1);
            break;
        }
        default:;
    }
    return;
}

// store assignment to the IO gate
bool PODEM_Generator::assign_pi(const uint &value, const long &netNum)
{
    for (auto it:this->inputWires)
    {
        if (it->IOWireNum == netNum)
        {
            if (value==20) // clean the assignment back to x
                it->valid = false;
            else
            {
                it->value = value;
                it->valid = true;
            }
            return 0;
        }
    }
    
    return 1; // no pi found!!
}

// broadcast primary input assignment, it will search in the IOWireNum and broadcast it
unsigned int PODEM_Generator::broadcast_pi()
{
    long fault_netNum;
    uint fault_val;
    fault_netNum = this->gl_flist.front()->netNum;
    fault_val = this->gl_flist.front()->fault_value;
    
    for (auto it:this->inputWires) // search in input wires
    {
        if (!it->valid)
            continue;
        else // find the Inputwires
        {
            for (auto iit:it->IOGate) // broadcast
            {
                if (iit->input1.wireNum == it->IOWireNum)
                {
                    iit->input1.valid = true;
                    if (fault_netNum == iit->input1.wireNum)
                        iit->input1.value = (it->value==fault_val)? fault_val: ((fault_val)? D_BAR:D_POS);
                    else
                        iit->input1.value = it->value;
                }
                
                if (iit->input2.wireNum == it->IOWireNum)
                {
                    iit->input2.valid = true;
                    if (fault_netNum == iit->input2.wireNum)
                        iit->input2.value = (it->value==fault_val)? fault_val: ((fault_val)? D_BAR:D_POS);
                    else
                        iit->input2.value = it->value;
                }
            }
        }
    }
    return 0;
}

// broadcast primary input assignment, it will search in the IOWireNum and broadcast it
unsigned int PODEM_Generator::broadcast_pi(const uint &value, const long &net_num)
{
    bool is_pi(false);
    
    for (auto it:this->inputWires) // search in input wires
    {
        if (it->IOWireNum != net_num)
            continue;
        else // find the Inputwires
        {
            is_pi = true;
            for (auto iit:it->IOGate) // broadcast
            {
                if (iit->input1.wireNum == net_num)
                {
                    if (iit->input1.valid && iit->input1.value != value)
                    {
                        printf("Error: broadcast pi conflict during backtrace...\n");
                        return 1; // inconsistent value assignment in primary input when backtrace
                    }
                    else
                    {
                        iit->input1.valid = true;
                        iit->input1.value = value;
                    }
                }
                
                if (iit->input2.wireNum == net_num)
                {
                    if (iit->input2.valid && iit->input2.value != value)
                    {
                        printf("Error: broadcast pi conflict during backtrace...\n");
                        return 1; // inconsistent value assignment in primary input when backtrace
                    }
                    else
                    {
                        iit->input2.valid = true;
                        iit->input2.value = value;
                    }
                }
            }
            break;
        }
    }
    
    if (is_pi)
        return 0;  // it is PI and assignment succeed
    else
        return 2;  // it is not PI
}

// return 0 if succed and return non-zero value when fail
unsigned int PODEM_Generator::backtrace(long netNum, uint value, uint &asi_pi_val, long &asi_pi_net )
{
    long i(0);
    uint tmp_value(value);
    NodePtr current_node(NULL); // point to current node
    
    for (auto it:this->inputWires) // check whether it is Primary input
        if (netNum == it->IOWireNum)
        {
            asi_pi_val = value;
            asi_pi_net = netNum;
            return 0;
        }
    
    // find the gate with exact wire number
    for (i=0; i<this->adjList.size();i++)
    {
        if (this->adjList[i]->output.wireNum == netNum)
        {
            if (is_gate_xpath(this->adjList[i]))  // pick one gate
                btQueue.push(std::make_pair(this->adjList[i],value));
        }
    }
    
    if (!btQueue.size()) // no xpath found from the given wireNum
    {
        printf("Error initialize btQueue during backtrace, cannot find gate...\n");
        return 3;
    }
    
    while(btQueue.size()) // loop until primary input
    {
        current_node = btQueue.front().first;
        tmp_value = btQueue.front().second;
        btQueue.pop();
        int control_value(-1); // initialize to an unreal value
        
        if (current_node->gate == OR || current_node->gate == NOR)
            control_value = 1;
        else if (current_node->gate == AND || current_node->gate == NAND)
            control_value = 0;
        
        if (current_node->gate == INV | current_node->gate == NAND | current_node->gate == NOR)
        {
            if (tmp_value == D_BAR || tmp_value == D_POS)
                tmp_value = (tmp_value == D_BAR)? D_POS: D_BAR;
            else
                tmp_value = !tmp_value; // add inversion parity
        }
        
        switch(current_node->gate)
        {
            case INV:
            case BUF:
            case NOR:
            case OR:
            case NAND:
            case AND:
            {
                i = is_gate_xpath(current_node);
                if (i==0) // this is not x-path and the value cannot propagate backward it
                    break;
                
                if (i==3) // i==3, both of inputs can be xpath, them randomly pick one
                    i = sel_random(1,2);
                
                if (i==1) // i==1, input1 can be xpath
                {
                    if (current_node->input1.is_pi)
                    {
                        asi_pi_net = current_node->input1.wireNum;
                        asi_pi_val = tmp_value;
                        return 0;
                    }
                }
                else if (i==2) // i==2, input2 can be xpath
                {
                    if (current_node->input2.is_pi)
                    {
                        asi_pi_net = current_node->input2.wireNum;
                        asi_pi_val = tmp_value;
                        return 0;
                    }
                }
                
                long next_netnum = (i==1)? current_node->input1.wireNum : current_node->input2.wireNum;
                
                for (auto it:current_node->lastGates)
                {
                    if ((it->output.wireNum==next_netnum) && (!it->output.valid | (it->output.valid&& it->output.value==tmp_value)))
                    {
                        btQueue.push(make_pair(it,tmp_value));
                        break;
                    }
                    else if ((it->output.valid & (it->output.value!=tmp_value)) & (it->output.wireNum==next_netnum))
                    {
                        printf("Error backtrace assignment conflict...\n");
                        return 1; // assignment conflict
                    }
                }
                break;
            }
            default:;
        }
    }
    
    return 0;
}

// backtrace for Primary output
unsigned int PODEM_Generator::backtrace_PO(long netNum, uint value)
{
    long i(0);
    uint tmp_value(value);
    NodePtr current_node(NULL); // point to current node
    i = broadcast_pi(value, netNum);
    
    if (!i) // this is PI and assignment success
        return 0;
    else if (i==1) // PI assigment conlict
        return 1;
    
    // find the gate with exact wire number
    for (i=0; i<this->adjList.size();i++)
    {
        if (this->adjList[i]->output.wireNum == netNum)
        {
            if (is_gate_xpath(this->adjList[i]))  // pick one gate
                btQueue.push(std::make_pair(this->adjList[i],value));
        }
    }
    
    if (!btQueue.size()) // no xpath found from the given wireNum
    {
        printf("Error initialize btQueue during backtrace, cannot find gate...\n");
        return 3;
    }
    
    while(btQueue.size()) // loop until primary input
    {
        current_node = btQueue.front().first;
        tmp_value = btQueue.front().second;
        btQueue.pop();
        uint control_value(255); // initialize to an unreal value
        
        if (current_node->gate == OR || current_node->gate == NOR)
            control_value = 1;
        else if (current_node->gate == AND || current_node->gate == NAND)
            control_value = 0;
        
        // assign value to current_node output
        current_node->output.value = tmp_value;
        current_node->output.valid = true;
        
        if (current_node->gate == INV | current_node->gate == NAND | current_node->gate == NOR)
        {
            if (tmp_value == D_BAR || tmp_value == D_POS)
                tmp_value = (tmp_value == D_BAR)? D_POS: D_BAR;
            else
                tmp_value = !tmp_value; // add inversion parity
        }
        
        switch(current_node->gate)
        {
            case INV:
            case BUF:
            case NOR:
            case OR:
            case NAND:
            case AND:
            {
                i = is_gate_xpath(current_node);
                if (i==0) // this is not x-path and the value cannot propagate backward it
                    break;
                
                if (i==3 && tmp_value == control_value) // i==3, both of inputs can be xpath, them randomly pick one
                    i = sel_random(1,2);
                
                if (i==1) // i==1, input1 can be xpath
                {
                    current_node->input1.valid = true;
                    current_node->input1.value = tmp_value;
                    int bc_res(0); // broadcast result
                    if (current_node->input1.is_pi)
                        bc_res = broadcast_pi(tmp_value, current_node->input1.wireNum);
                    if (bc_res==1)
                        return 1; //PI assignment conflict
                    else if (bc_res == 2)
                        return 2; // cannot find it in inputwires
                }
                else if (i==2) // i==2, input2 can be xpath
                {
                    current_node->input2.valid = true;
                    current_node->input2.value = tmp_value;
                    int bc_res(0); // broadcast result
                    if (current_node->input2.is_pi)
                        bc_res = broadcast_pi(tmp_value, current_node->input2.wireNum);
                    if (bc_res==1)
                        return 1; //PI assignment conflict
                    else if (bc_res == 2)
                        return 2; // cannot find it in inputwires
                }
                
                if (i==3)
                {
                    current_node->input2.valid = true;
                    current_node->input1.valid = true;
                    current_node->input1.value = tmp_value;
                    current_node->input2.value = tmp_value;
                    
                    int bc_res(0); // broadcast result
                    if (current_node->input1.is_pi)
                        bc_res = broadcast_pi(tmp_value, current_node->input1.wireNum);
                    if (bc_res==1)
                        return 1; //PI assignment conflict
                    else if (bc_res == 2)
                        return 2; // cannot find it in inputwires
                    
                    if (current_node->input2.is_pi)
                        bc_res = broadcast_pi(tmp_value, current_node->input2.wireNum);
                    if (bc_res==1)
                        return 1; //PI assignment conflict
                    else if (bc_res == 2)
                        return 2; // cannot find it in inputwires
                    
                    for (auto it:current_node->lastGates)
                    {
                        if ((!it->output.valid | (it->output.valid&& it->output.value==tmp_value)))
                            btQueue.push(make_pair(it,tmp_value));
                        else if ((it->output.valid&& it->output.value!=tmp_value))
                            return 1; // assignment conflict
                    }
                }
                else
                {
                    long next_netnum = (i==1)? current_node->input1.wireNum : current_node->input2.wireNum;
                    
                    for (auto it:current_node->lastGates)
                    {
                        if ((it->output.wireNum==next_netnum) && (!it->output.valid | (it->output.valid&& it->output.value==tmp_value)))
                            btQueue.push(make_pair(it,tmp_value));
                        else if ((it->output.valid&& it->output.value!=tmp_value))
                            return 1; // assignment conflict
                    }
                    break;
                }
            }
            default:;
        }
    }
    return 0;
}


void PODEM_Generator::print_input(std::ofstream &outFile)
{
    for (auto it:inputWires)
    {
        if (it->IOWireNum == it->IOGate[0]->input1.wireNum)
        {
            if (it->IOGate[0]->input1.valid)
                outFile<<static_cast<int>(it->IOGate[0]->input1.value);
            else
                outFile<<"x";
        }
        else if (it->IOWireNum == it->IOGate[0]->input2.wireNum)
        {
            if (it->IOGate[0]->input2.valid)
                outFile<<static_cast<int>(it->IOGate[0]->input2.value);
            else
                outFile<<"x";
        }
    }
    outFile<<std::endl;
}

void PODEM_Generator::print_input_assi(std::ofstream &outFile)
{
    for (auto it:inputWires)
    {
        if (it->valid)
            outFile<<static_cast<int>(it->value);
        else
            outFile<<"x";
    }
    outFile<<std::endl;
}

// return 0 if success, return 1 if failure
bool PODEM_Generator::podem_sim(std::vector<DF_NodePtr> D_frontier)
{
    uint fault, bt_value;
    long net_num, bt_netNum;

    if (!fault_in_po())
        return 0;
    
    if (!test_possible_check(D_frontier)) // test is not possible
        return 1;
    
    objective(net_num, fault,D_frontier);

    if (backtrace(net_num, fault, bt_value, bt_netNum)!=0)
        return 1;
    imply(bt_value,bt_netNum);

    if (!podem_sim(D_frontier))
        return 0; // success
    imply(!bt_value,bt_netNum);

    if (!podem_sim(D_frontier))
        return 0; // success
    imply(20,bt_netNum); // clean the assignment
    return 1; // return failure
}

void PODEM_Generator::generate(const string &cirName){
    string s(35,'-');
    
    ofstream outFile("./result/tv_"+cirName);
    outFile<<s<<endl;
    
    while (!gl_flist.empty())
    {
        generatorReset();
        std::vector<DF_NodePtr> D_frontier;   // D_frontier
        if (mf_is_po()) // return true if main fault is in Primary output
        {
            outFile<<"Podem find test vector for "<<gl_flist.front()->netNum<<" stuck-at-"<<gl_flist.front()->fault_value<<"\n";
            print_input(outFile);
            outFile<<s<<"\n";
            gl_flist.pop();
            continue;
        }
        
        if (!podem_sim(D_frontier))
        {
            outFile<<"Podem find test vector for "<<gl_flist.front()->netNum<<" stuck-at-"<<gl_flist.front()->fault_value<<"\n";
            print_input_assi(outFile);
        }
        else
        {
            outFile<<"Podem cannot find a test vector for "<<gl_flist.front()->netNum<<" stuck-at-"<<gl_flist.front()->fault_value<<std::endl;
        }
        outFile<<s<<"\n";
        
        gl_flist.pop();
    }
    
    outFile.close();
    
}


