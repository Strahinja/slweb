for d in *; do
    [ -d $d ] && echo $d/index.html
done | xargs redo-ifchange

