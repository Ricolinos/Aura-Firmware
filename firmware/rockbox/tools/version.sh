#!/bin/sh
#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

# Usage: version.sh [source-root]

# Prints the revision of the repository.
#
# The format is rNNNNNNNNNN[M]-YYMMDD
#
# The M indicates the revision has been locally modified
#
# This logic is pulled from the Linux's scripts/setlocalversion (also GPL)
# and tweaked for rockbox.
gitversion() {
    # This verifies we are in a git directory
    if head=`git -C "$1" rev-parse --verify --short=10 HEAD 2>/dev/null`; then

	# Are there uncommitted changes?
	#
	# Aura (D-354): the upstream `export GIT_WORK_TREE="$1"` assumes "$1"
	# (the rockbox source root) IS the git repository root. In this
	# fork it is a SUBDIRECTORY of a larger repo (firmware/rockbox/
	# inside Aura-Firmware, see MODIFICATIONS.md/D-002) with no .git of
	# its own. Forcing GIT_WORK_TREE to that subdirectory while GIT_DIR
	# is discovered by walking up to the outer repo's .git makes git
	# compare every tracked path (relative to the OUTER root) against
	# files that would have to exist relative to the subdirectory --
	# none of them do, so `git diff` reports the entire outer repo as
	# "changed", and every build gets a false "M" regardless of actual
	# dirtiness (measured: even a freshly-committed, clean tree). A
	# pathspec restricting the diff to "$1" gives the same "does the
	# rockbox source under here differ from HEAD" answer without
	# needing GIT_WORK_TREE at all -- it works whether "$1" is the repo
	# root (upstream's case) or a subdirectory of one (this fork's).
	if git -C "$1" diff --name-only HEAD -- . | read dummy; then
	    mod="M"
	elif git -C "$1" diff --name-only --cached HEAD -- . | read dummy; then
	    mod="M"
	fi

	echo "${head}${mod}"
	# All done with git
	exit
    fi
}

#
# First locate the top of the src tree (passed in usually)
#
if [ -n "$1" ]; then TOP=$1; else TOP=..; fi

# setting VERSION var on commandline has precedence
if [ -z $VERSION ]; then
    # If the VERSIONFILE exisits we use that
    VERSIONFILE=docs/VERSION
    if [ -r $TOP/$VERSIONFILE ]; then
        VER=`cat $TOP/$VERSIONFILE`;
    else
        # Ok, we need to derive it from the Version Control system
	    VER=`gitversion $TOP`
    fi
if [ -z $SOURCE_DATE_EPOCH ]; then
	VERSION=$VER-`date -u +%y%m%d`
else
	VERSION=$VER-`date -d @$SOURCE_DATE_EPOCH -u +%y%m%d`
fi
fi
echo $VERSION

