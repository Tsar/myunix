#!/bin/bash

cat << EOF > life.sh
#!/bin/bash

sed -e '
N
N
N

EOF

for i in `seq 0 65535`
do
    echo -n "$i: "
    for j in `seq 15 -1 0`
    do
        let "k=2**$j"
        if [ $[i & k] -eq "$k" ]
        then
            echo -n "1"
        else
            echo -n "0"
        fi
    done
    echo " b"
done

cat << EOF >> life.sh
s/0011\n1011\n0010\n0000/aabb\nccdd\nmmmm\nnnnn/; tend
s/0011\n1011\n0010\n0001/aabb\nccdd\nmmmm\nnnnn/; tend
s/aabb\nccdd\nmmmm\nnnnn/xxxx/; tend
EOF

cat << EOF >> life.sh

:end
' field.txt
EOF
