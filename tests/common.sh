#!/bin/bash

{
    exec 3>&2
}

function echo()
{
    local nl='\n' no_new_line=0

    if [[ $1 == -n ]]; then
        no_new_line=1
        shift
    fi

    (( no_new_line )) && nl=''
    printf "%s$nl" "$*"
}

function info()
{
    printf '++ %s\n' "$*"
}

function fatal()
{
    local errno=1 msg

    if [[ -n $1 && -z ${1//[0-9]/} ]]; then
        errno=$1
        shift
    fi

    if [[ -n $1 ]]; then
        echo "!! $*"
    fi

    exit $errno
}

function run()
{
    echo "# $*" >&3
    "$@"
}

function assert()
{
    echo "# $*"
    eval "$*" || fatal "ASSERT: $*"
}

function negass()
{
    echo "# $*"
    ! eval "$*" || fatal "NEG_ASSERT: $*"
}

function assert_run()
{
    run "$@" || fatal "ASSERT: $*"
}

function negass_run()
{
    ! run "$@" || fatal "NEG_ASSERT: $*"
}

{
    assert "[[ -d $BINDIR && -d $SRCDIR ]]"
    assert "[[ -x $BINDIR/sexpect ]]"

    true
}
