for f in *.slw inc/*.slw; do
    echo $f
done | xargs redo-ifchange

