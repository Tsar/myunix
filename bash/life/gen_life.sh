#!/bin/bash

cat << EOF > life.sh
#!/bin/bash

sed -e '
N
N
N

EOF

cat << EOF >> life.sh
s/0011\n1011\n0010\n0000/aabb\nccdd\nmmmm\nnnnn/; tend
s/0011\n1011\n0010\n0001/aabb\nccdd\nmmmm\nnnnn/; tend
s/aabb\nccdd\nmmmm\nnnnn/xxxx/; tend
EOF

cat << EOF >> life.sh

:end
' field.txt
EOF
