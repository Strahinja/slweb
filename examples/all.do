for d in *; do
    [ -d $d ] && echo $d/all
done | xargs redo-ifchange

