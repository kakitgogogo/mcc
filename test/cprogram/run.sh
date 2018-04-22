#/bin/bash

for file in *
do
    if [ -d $file ]
    then
        echo "==================== "$file" ======================"
	cd $file
    	./run.sh
	cd ..
	echo "==================================================="
	echo
    fi
done    
