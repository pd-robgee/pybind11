
DEFAULT_CXX=""

kern="`uname -s`"
if which g++ >/dev/null; then
    if [ "$kern" = "Darwin" ]; then
        # Don't include g++ on Darwin by default unless it really is g++; the
        # default isn't really g++, it's a wrapper around an LLVM backend.
        # Real g++ has --version output starting with 'g++'
        if g++ --version | head -n 1 | grep -q '^g++'; then
            DEFAULT_CXX="$DEFAULT_CXX g++"
        fi
    else
        DEFAULT_CXX="$DEFAULT_CXX g++"
    fi
fi

if which clang++ >/dev/null; then
    DEFAULT_CXX="$DEFAULT_CXX clang++"
fi


CXXS="${CXX:-$DEFAULT_CXX}"

python_flags="$(
    for pycfg in python3-config python-config python2-config; do
        if which $pycfg >/dev/null; then
            $pycfg --includes
            $pycfg --ldflags
            break
        fi
    done | tr '\n' ' ')"

CXXFLAGS="${CXXFLAGS:--Iinclude $python_flags -O2 -DNDEBUG -fPIC -std=c++14 -flto -fvisibility=hidden -Wall -Wextra -Wconversion}"

branch="`git rev-parse --abbrev-ref HEAD`"
commit="`git rev-parse --short HEAD`"

gnutime=
if [ "`uname -s`" = "Linux" ] && which time >/dev/null; then
    gnutime=1
fi

time_command() {
    if [ -n "$gnutime" ]; then
        # Use system `time` on linux to also report max RSS, which bash's built-in can't do
        \time --format='%U user, %e elapsed, %Mk max' "$@"
    else
        TIMEFORMAT="%U user, %E elapsed"
        time "$@"
    fi
}


master=
if [ "$branch" = "upstream" ] || [ "$branch" = "master" ]; then master=1; fi

pad() {
    printf "%${1}s" "$2"
}

show_compilers() {
    for CXX in $CXXS; do
        echo -n "$(pad 11 $CXX) is: "
        $CXX --version | head -n 1
    done
}


title() {
    echo -e "Benchmarking $1 on $branch ($commit)\n---"

    show_compilers

    echo
}

