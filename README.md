# PODEM Generator
PODEM Test Generator for 6140

[![License](https://img.shields.io/badge/license-BSD-blue)](https://github.com/yankanghong/PODEM_Generator/blob/master/LICENSE)

## Github
Go to following link and see the readme!
[PODEM Test Generator](https://github.com/yankanghong/PODEM_Generator)

## File Hierachy
Folder | Function
:---------------: | :---------------:
[circuits](/circuits) | Folder of circuit files  
[faultList](/faultList) | Folder of fault list files       
[result](/result) | Simulation results (generated test vectors) will be save to this folder under `mode 1` or `mode 2`  


## Run the PODEM generator
Open terminal and `cd` to current directory.
Type `-h` or `-help` to see usage
``` zsh
./run.sh -h

```
### Test All Circuits
This command will generate test vectors for `s27`, `s298f_2`, `s343f_2` and `s349f_2` with fault lists in [faultList](/faultList) and save results to [result](/result)

``` zsh
./run.sh all

```

### Test One Circuit
This commands will generate test vectors for one circuit at a time. The path for both circuit and fault list must be specified.

``` zsh
./run.sh -ckt ./circuits/<circuit_name>.txt -fl ./faultList/<fault_list_name>.txt

```

### Clean all data
This command will clean all simulation data in `./result` and `./plot` folders
``` zsh
./run.sh clean

```

## Check runtime of the simulator
To get the runtime of this simulator, open `src/main.cpp` and on the top of the file, uncomment 
``` c++
// #define CHECK_RUNTIME
```
into 
``` c++
#define CHECK_RUNTIME
```