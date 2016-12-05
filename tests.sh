#!/bin/bash

set -u
set -e

hostbase="izcip"
domain=ibr.cs.tu-bs.de
term="xterm -hold -e"
portbase=50

echo "please enter y-number:"
read user

# create root server
${term} "ssh ${1}@${hostbase}01 ~/ibrc/ibrcd" &

connected=(1)
# connect other servers
for i in `seq -f "%02g" $1 $2 | shuf `; do
  destnum=`shuf -e ${connected} | head -n 1`
  ibr-wake ${hostbase}$i
  ${term} "ssh ${user}@${hostbase}$i ~/ibrc/ibrcd -h ${hostbase}${destnum} -k ${portbase}$i" &
  connected+=($i)
done

# connect clients
for i in `seq -f "%02g" $3 $4`; do
  destnum=`shuf -e ${connected} | head -n 1`
  ibr-wake ${hostbase}$i
  ${term} "ssh ${user}@${hostbase}$i ~/ibrc/ibrcc ${hostbase}${destnum} ${portbase}00" &
done
