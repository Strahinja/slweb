for f in *.slw inc/*.slw; do
    echo $f
done | xargs redo-ifchange
../../slweb -d $(dirname $2.slw) $(basename -s.html $2).slw >$3

