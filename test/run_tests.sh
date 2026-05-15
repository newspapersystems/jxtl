#!/usr/bin/env bash

check_status() {
    if [ $? -ne 0 ] ; then
        echo $1
        exit 1
    fi
}

run_test() {
    local dir=$1
    local args=$2
    $jxtl $args -t $dir/input > $dir/test.output
    check_status "jxtl with args $args had bad exit status in $dir"
    case "$dir" in
    ./t7)
        diff <(sort "$dir/output") <(sort "$dir/test.output") > /dev/null 2>&1
        check_status "Failed test in $dir"
        ;;
    *)
        diff $dir/output $dir/test.output > /dev/null 2>&1
        check_status "Failed test in $dir"
        ;;
    esac
    rm $dir/test.output
}

rm -f t.json
$xml2json < t.xml > t.json
check_status "failed to convert test XML to JSON"

for dir in `find . -mindepth 1 -type d` ; do
    if [ -f $dir/input ] ; then
        run_test $dir "-s -x t.xml"
        run_test $dir "-j t.json"
    fi
done

exit 0
