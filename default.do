if [ -r $1.in ]; then
    redo-ifchange $1.in version date
    read VERSION <version
    read DATE <date
    sed -e "s/%VERSION%/$VERSION/g" \
        -e "s/%DATE%/$DATE/g" <$1.in
else
    echo "$0: don't know how to build '$1'" >&2
    exit 99
fi

