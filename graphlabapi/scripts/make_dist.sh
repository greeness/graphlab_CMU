#!/bin/bash

major_version=1

echo "THIS MUST BE RUN IN GRAPHLAB HOME"

## JOEY: WHY ARE WE REMOVING THE FOLDER AND THEN USING RSYNC?
rm -fR dist/graphlabapi
mkdir -p dist/graphlabapi
rsync -vv -al --delete --delete-excluded \
    --exclude=/debug --exclude=/release --exclude=/profile --exclude=/apps \
    --exclude=.hg --exclude=/doc/doxygen --exclude=/matlab --exclude=/extapis \
    --exclude=/dist --exclude=/deps --exclude=*~ --exclude=*.orig --exclude=/configure.deps \
    --exclude /LGPL_prepend.txt  --exclude /make_dist * dist/graphlabapi/.

mkdir dist/graphlabapi/apps
cp dist/graphlabapi/demoapps/CMakeLists.txt dist/graphlabapi/apps/
version=`hg summary | grep parent | sed 's/parent: //g' | sed 's/:.*//g'`
version="v${major_version}_$version"
echo "Version: $version"


cd dist
tar -vz \
    -cf graphlabapi_${version}.tar.gz \
    graphlabapi
    

ls -al graphlabapi/dist | tail -n 1

