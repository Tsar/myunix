#!/bin/bash

make_pwd() {
    pwd=$i
    while [ ${#pwd} -ne 4 ]
    do
        pwd="0$pwd"
    done
}

i=0
make_pwd
unlocked=0

while [ $unlocked -eq 0 ]
do
    echo "try: $pwd"
    rm -f tex2html-test.tar
    if 7z x -p$pwd tex2html-test.tar.7z >/dev/null 2>&1
    then
        echo "pass $pwd is correct"
        unlocked=1
    else
        i=$[i + 1]
        make_pwd
        if [ $i -eq 10000 ]
        then
            echo "pass not decrypted"
            exit 1
        fi
    fi
done

echo "extracting subarchive"
tar -xf tex2html-test.tar

echo "all extractions done"

exit 0
