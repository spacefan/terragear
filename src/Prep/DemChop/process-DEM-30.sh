#!/bin/sh
#

TOOLPATH=`pwd`
SOURCE=/stage/fgfs01/curt/RawData/DEM-30-Ascii
SCRATCH=/tmp/Scratch
WORK=/stage/fgfs04/curt/Work/DEM-30-New

if [ -z $1 ]; then
    echo "usage: $0 base-dem-30-ascii-file"
    exit
fi

if [ ! -d $SCRATCH ]; then
    mkdir -p $SCRATCH
fi

cd ${SCRATCH} || exit
echo "cd to $SCRATCH successful"

tar xzvf ${SOURCE}/${1}.tar.gz

for i in ${SCRATCH}/$1/*.dem; do
    ${TOOLPATH}/demchop $i $WORK >> ${TOOLPATH}/log-$1 2>&1
done

echo rm -rf ${SCRATCH}
rm -rf ${SCRATCH}
