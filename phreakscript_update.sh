#!/bin/sh
g() { printf "\e[32;1m%s\e[0m\n" "$*" >&2; }
err() { printf "\e[31;1m%s\e[0m\n" "$*" >&2; } # https://stackoverflow.com/questions/2990414/echo-that-outputs-to-stderr
v1=`grep "# v" $2 | head -1 | cut -d'v' -f2`
tmp=/tmp/phreaknet.sh
src=$1
if [ "$src" = "https://docs.phreaknet.org/script" ]; then
	src="https://docs.phreaknet.org/script/phreaknet.sh" # needed for compatability with < 0.0.38, can be removed eventually
fi
printf "Upstream: %s\n" "$src"
wget -q $src -O $tmp --no-cache # always replace
chmod +x $tmp
test=`$tmp help 2>&- | grep "PhreakScript" | grep "(" | wc -l`
if [ "$test" = "1" ]; then
	mv $tmp $2
	v2=`grep "# v" $2 | head -1 | cut -d'v' -f2`
	g "Successfully updated PhreakScript from $v1 to $v2"
else
	err "PhreakScript failed to update - upstream contains errors ($test)."
	rm "$tmp"
fi
exec rm -f "$3"
