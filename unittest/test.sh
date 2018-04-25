for file in *
do
    filename=$(basename "$file")
    extension="${filename##*.}"
    target="${filename%.*}"
    if [ "$extension" == "c" ]
    then
        mcc -o $target $file
        ./$target
        rm $target
    fi
done    