#!/bin/bash
set -x
git pull
git commit -am "$1"
git push origin master
