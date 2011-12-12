#!/bin/bash
make_pwd() {
    pwd=$i
    while [ ${#pwd} -ne 4 ]
    do
        pwd="0$pwd"
    done
}

if [ $# -gt 0 ]
then
    startI=$1
else
    startI=0
    ./uncrypt.sh 1000 &
    ./uncrypt.sh 2000 &
    ./uncrypt.sh 3000 &
    ./uncrypt.sh 4000 &
    ./uncrypt.sh 5000 &
    ./uncrypt.sh 6000 &
    ./uncrypt.sh 7000 &
    ./uncrypt.sh 8000 &
    ./uncrypt.sh 9000 &
fi

i=$startI
make_pwd
unlocked=0

while [ $unlocked -eq 0 ]
do
    echo "try: $pwd"
    rm -f tex2html-test.tar
    if 7z x -p$pwd tex2html-test.tar.7z >/dev/null 2>&1
    then
        echo "pass $pwd is correct"
        echo "$pwd" > password.txt
        unlocked=1
    else
        i=$[i + 1]
        make_pwd
        if [ ${i:-3} -eq 000 ]
        then
            echo "search in interval [$startI; $i) unsuccessfull"
        fi
    fi
done
