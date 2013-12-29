#!/bin/sh
if [ ! -d "$HOME/.eduke32" ]
then
	mkdir "$HOME/.eduke32"
fi

if [ ! -f "$HOME/.eduke32/duke3d.grp" ]
then
	echo "No duke3d.grp data file found!"
	./missingData.elf
	exit 1
else
	./eduke32.elf
fi
