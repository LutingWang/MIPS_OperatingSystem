#########################################################################
# File Name: push.sh
# Author: Luting Wang
# mail: 2457348692@qq.com
# Created Time: 2019年04月21日 星期日 12时48分44秒
#########################################################################
#!/bin/bash

if [[ $# < 2 ]]; then
        echo error
        exit
fi
git add --all
git commit -a -m "\"$1\""
git push origin $2:$2

