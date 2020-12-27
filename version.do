if ! git describe >$3; then
    echo "$0: can't call git describe, falling back to 'unknown'" >&2
    echo 'unknown' >$3
fi
redo-always
redo-stamp <$3

