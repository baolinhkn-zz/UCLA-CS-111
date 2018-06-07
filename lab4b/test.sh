#!/bin/bash

./lab4b --period=2 --scale=C --log="log.txt" <<-EOF
SCALE=F
PERIOD=1
START
STOP
LOG hello
OFF
EOF

ret=$?
if [ $ret -ne 0 ]
then
    echo "lab4b executable does not process input!"
fi
