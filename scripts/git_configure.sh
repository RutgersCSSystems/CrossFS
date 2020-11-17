#!/bin/bash
BRANCH="master"
"Configure scripts to repo"
git config credential.helper store
git config --global user.name $1
echo "a" > test.txt
git add test.txt
git commit -am "test"
git rm test.txt
git push origin $BRANCH
