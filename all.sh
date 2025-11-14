#!/bin/bash

for name in $(find . -type f); do
    if [ $name -eq "allfiles.txt" ] ; then
        continue
    fi
    echo $name
    cat $name
    echo -e "\n--------------------------------\n"
done
