
. `dirname $0`/bench-common.sh

if [ "$kern" = "Darwin" ]; then
    strip_args="-u -r"
fi

compile() {
    set -e
    set -o pipefail
    SRC="$1"
    for CXX in $CXXS; do
        SO="${SRC%.*}_${CXX}.so"
        echo -n "$(pad 11 $CXX), $(pad 4 $CLASSES) classes compilation: "
        if ! time_command $CXX $CXXFLAGS -shared -o $SO $SRC \
            2>&1 | perl -ne '/ user, / and print s/\n//r'; then
            echo -e "Compilation failed, aborting.  Tried to compile using:\n\n$CXX $CXXFLAGS -shared -o $SO $SRC\n"
            return 1
        fi

        strip $strip_args $SO 2>/dev/null
        # stat's arguments vary across systems, so use perl to get so size
        echo ", $(perl -e 'print -s shift' $SO) bytes so"

    done
}
