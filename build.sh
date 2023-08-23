#!/bin/bash

cmake -G Xcode -B build-iphoneos \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_Swift_COMPILER_FORCED=true \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

cmake -G Xcode -B build-iphonesimulator \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_Swift_COMPILER_FORCED=true \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

cd build-iphoneos
xcodebuild
cd ..

cd build-iphonesimulator
xcodebuild
cd ..
