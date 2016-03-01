#!/bin/bash

if [[ $# != 1 ]]; then
    echo "Usage: handin.sh <student no.>"
    exit
fi

no=$1

git archive --format=tar --prefix=$no/ HEAD | gzip >$no.tar.gz
echo "Please send $no.tar.gz to aos.ppi.fudan@gmail.com"

