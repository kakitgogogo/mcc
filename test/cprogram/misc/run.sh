#/bin/bash

for file in *
do
    filename=$(basename "$file")
    extension="${filename##*.}"
    target="${filename%.*}"
    if [ "$extension" == "c" ]
    then
        echo "==================== "$filename" ======================"
        mcc -o $target $file
        ./$target
        rm $target
    fi
done    
