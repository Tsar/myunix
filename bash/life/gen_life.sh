#!/bin/bash

cat << EOF > life.sh
#!/bin/bash

sed -e '
N
N
N

EOF

neighbours[0]="15 12 13 3 1 7 4 5"
neighbours[1]="12 13 14 0 2 4 5 6"
neighbours[2]="13 14 15 1 3 5 6 7"
neighbours[3]="14 15 12 2 0 6 7 4"
neighbours[4]="3 0 1 7 5 11 8 9"
neighbours[5]="0 1 2 4 6 8 9 10"
neighbours[6]="1 2 3 5 7 9 10 11"
neighbours[7]="2 3 0 6 4 10 11 8"
neighbours[8]="7 4 5 11 9 15 12 13"
neighbours[9]="4 5 6 8 10 12 13 14"
neighbours[10]="5 6 7 9 11 13 14 15"
neighbours[11]="6 7 4 10 8 14 15 12"
neighbours[12]="11 8 9 15 13 3 0 1"
neighbours[13]="8 9 10 12 14 0 1 2"
neighbours[14]="9 10 11 13 15 1 2 3"
neighbours[15]="10 11 8 14 12 2 3 0"

for i in `seq 0 65535`
do
    echo "Generating $i of 65535"
    for j in `seq 0 15`
    do
        let "k=2**$j"
        #let "x=$j%4"
        #y=$[j/4]
        if [ $[i & k] -eq "$k" ]
        then
            f[$j]=1
        else
            f[$j]=0
        fi
    done
    field="${f[0]}${f[1]}${f[2]}${f[3]}\n${f[4]}${f[5]}${f[6]}${f[7]}\n${f[8]}${f[9]}${f[10]}${f[11]}\n${f[12]}${f[13]}${f[14]}${f[15]}"

    for j in `seq 0 15`
    do
        c=0
        for neighbour in "${neighbours[$j]}"
        do
            if [ "${f[$neighbour]}" -eq 1 ]
            then
                c=$[c + 1]
            fi
        done
        if [ "$c" -eq 3 ]
        then
            newF[$j]=1
        else
            if [ "${f[$j]}" -eq 1 ] && [ "$c" -eq 2 ]
            then
                newF[$j]=1
            else
                newF[$j]=0
            fi
        fi
    done
    newField="${newF[0]}${newF[1]}${newF[2]}${newF[3]}\n${newF[4]}${newF[5]}${newF[6]}${newF[7]}\n${newF[8]}${newF[9]}${newF[10]}${newF[11]}\n${newF[12]}${newF[13]}${newF[14]}${newF[15]}"

    echo "s/$field/$newField/; tend" >> life.sh
done

#cat << EOF >> life.sh
#s/0011\n1011\n0010\n0000/aabb\nccdd\nmmmm\nnnnn/; tend
#s/0011\n1011\n0010\n0001/aabb\nccdd\nmmmm\nnnnn/; tend
#s/aabb\nccdd\nmmmm\nnnnn/xxxx/; tend
#EOF

cat << EOF >> life.sh

:end
' field.txt
EOF
