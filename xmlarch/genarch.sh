#!/bin/sh
xsltproc --xinclude --nonet nocomment.xsl $1.xml | \
	xsltproc --nonet --output $1-full.xml arch.xsl -
rnv arch.rnc $1-full.xml
