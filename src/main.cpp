/*
 Author: Kanghong Yan
 Class: ECE6140
 Last Date Modified: 11/12/2019
 Description: main program of PODEM generator
 */

#include "PODEM_generator.h"
#include "PODEM_generator.cpp"

// enable the runtime checking
#define CHECK_RUNTIME
#ifdef CHECK_RUNTIME
    #include <ratio>
    #include <chrono>
#endif

using namespace std;

void die_usage(char *argv) {
    printf("Usage: %s [options] <argv> \n", argv);
    printf("Options: \n");
    printf("    -ckt  <filename>  Circuit filename\n");
    printf("    -fl   <filename>  Fault list filename\n");
    printf("    -help / -h        Get usage\n\n");
    exit(EXIT_FAILURE);
}

string print_info(const string &cirFile, const string &flFile) {
    string splitBar(60,'-');
    string cirName;
    cout<<splitBar<<endl;
    cout<< "*    PODEM Test Generator"<<endl;
    cout<< "*    Author: Kanghong Yan"<<endl;

    int index(static_cast<int>(cirFile.find_last_of('/')));

    cirName = cirFile.substr(index+1,cirFile.length());
    cout<<"*    Circuit File:   "<<cirName<<endl;

    index = static_cast<int>(flFile.find_last_of('/'));
    cout<<"*    Fault List File: "<<flFile.substr(index+1,flFile.length())<<endl;
    
    cout<<splitBar<<endl;
    return cirName;
}

int main(int argc, char* argv[]) {

    string cirFile(" "), flFile(" ");
    ofstream outFile;
    string cirName;

    if (argc < 2) {
        cout<< "input args wrong, please check usage:"<<endl;
        die_usage(argv[0]);
    }
    else {
        int i=0;
        for (i=1; i<argc; i++) {
            string arg;
            arg = argv[i];
            if (arg == "-ckt") {
                if (i < argc-1)
                    cirFile = argv[++i];
            }
            else if (arg == "-fl") {
                if (i < argc-1)
                    flFile = argv[++i];
            }
            else
                die_usage(argv[0]);
        }
        
    }
    
    // print running info
    cirName = print_info(cirFile, flFile);    

#ifdef CHECK_RUNTIME
    chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
#endif
    
    // initialize a test generator
    PODEM_Generator test_generator(cirFile, flFile);

#ifdef CHECK_RUNTIME
    chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();

    chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(end - start);
    cout << "It takes "  << time_span.count()*1000 << " ms to build the circuit"<<endl;
    cout << endl;
#endif

    // run the test generator
    test_generator.generate(cirName);
    return 0;
}
