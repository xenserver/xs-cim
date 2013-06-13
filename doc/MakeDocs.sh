#!/bin/sh
# Generates the documentation folder for a build
# requires a running instance of the CIM server
# args: 1 - CIM server IP address
#       2 - CIM username
#       3 - CIM password
DOCBUILDDIR=/tmp
DOCDIR=$DOCBUILDDIR/xs-cim-docs
CLASSDOCS=$DOCDIR/docs
rm -rf $DOCDIR
rm -f $DOCBUILDDIR/xs-cim-docs.tar.gz

mkdir $DOCDIR
mkdir $CLASSDOCS

# Copy the readme
cp README.html $DOCDIR

# Copy the sample source code
echo "<html><body><pre>" > $DOCDIR/Python-Sample.html
cat ./Python-Sample.py >> $DOCDIR/Python-Sample.html
echo "</pre></body></html>" >> $DOCDIR/Python-Sample.html

echo "<html><meta http http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/><body><pre>" > $DOCDIR/WSMan-Sample.html
# handle the < and > with HTML escapes
cat ./WSManSample/Program.cs | sed 's/</\&lt;/g' | sed 's/>/\&gt;/g'  >> $DOCDIR/WSMan-Sample.html
echo "</pre></body></html>" >> $DOCDIR/WSMan-Sample.html

python ./GenerateClassDocs.py ../schema/Xen_DefaultNamespace.regs $CLASSDOCS $1 $2 $3

pushd .
cd $DOCBUILDDIR
tar -czf xs-cim-docs.tar.gz xs-cim-docs
popd

echo "Docs are available at $DOCBUILDDIR/xs-cim-docs.tar.gz"
