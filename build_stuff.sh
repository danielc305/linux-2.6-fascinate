#!/bin/bash

MODELS="fascinate mesmerize showcase"
WORK=`pwd`
CONTINUE="n"

doit()
{
	echo "$CMD"
	eval "$CMD" 1>"$WORK"/stdlog.txt 2>"$WORK"/errlog.txt
	if [ $? != 0 ]; then
		echo -e "FAIL!\n"
		if [ "$CONTINUE" != "y" ]; then
			exit 1
		fi
	else
		echo -e "Success!\n"
		rm -f "$WORK"/*log.txt
	fi
}

fetch_repo()
{
	echo "***** Fetching code for \"$REPO\" *****"
	if [ ! -d "$REPO"/.git ]; then
		rm -rf "$REPO"
		CMD="git clone git://github.com/jt1134/\"$REPO\"" && doit
	else
		cd "$REPO"
		git remote add origin git://github.com/jt1134/"$REPO".git >/dev/null 2>&1
		CMD="git fetch origin" && doit
		CMD="git merge origin/voodoo-dev" && CONTINUE="y" && \
		if ! doit; then
			echo "***** Problem merging \"$REPO\". Redownloading... *****"
			rm -rf "$REPO"
			# loop once P:
			CONTINUE="n" && fetch_repo "$REPO"
		fi
		cd ..
	fi
	CONTINUE="n"
}

