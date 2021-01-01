#!/bin/bash

for i in *.c *.h
do
  if ! grep -q Copyright $i
  then
    cat copyright.txt $i >$i.new && mv $i.new $i
  fi
done

