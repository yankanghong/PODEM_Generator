# filepath=$1
# filename="${filepath##*/}"
# fileFolder="${filepath%/*}"
# pureFilename="${filename%%.*}"
# extension=$(echo $filename | cut -d . -f2)
filepath="./src/main.cpp"

echo "#[INFO] Date: $(date)"

if [ "$1" = "clean" ]
then
    echo "#[INFO] run clean..."    
    rm -r result
    mkdir result
elif [ "$1" = "all" ]
then
    echo "#[INFO] run all ckts ..."
    echo "#[INFO] compile with g++ (c++11 support needed)"
    g++ -g -Wall -std=c++11 -O2 $filepath -o main
    echo
        ./main -ckt ./circuits/s27.txt -fl ./faultList/s27_fl.txt
        ./main -ckt ./circuits/s298f_2.txt -fl ./faultList/s298f_2_fl.txt
        ./main -ckt ./circuits/s344f_2.txt -fl ./faultList/s344f_2_fl.txt
        ./main -ckt ./circuits/s349f_2.txt -fl ./faultList/s349f_2_fl.txt
elif [[ "$1" = "test" ]]
then
    echo "#[INFO] run test ckt ..."
    echo
    g++ -g -Wall -std=c++11 -O2 $filepath -o main
    ./main -ckt ./circuits/test.txt -fl ./faultList/test_fl.txt

else
    echo "#[INFO] run a single ckt ..."
    echo
    g++ -g -Wall -std=c++11 -O2 $filepath -o main
    echo 
    if [ $# -eq 4 ]
    then
        ./main $1 $2 $3 $4
    else
        echo "wrong arguments"
        ./main -help
    fi
fi

if [ -f "main" ]
then
    echo "exit simulation..."
    rm main
fi

if [ -d "main.dSYM" ]
then
    echo
    echo "#[INFO] shell: delete temporary .dSYM file ..."
    rm -r main.dSYM
fi
