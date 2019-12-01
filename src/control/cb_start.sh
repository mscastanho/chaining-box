#!/usr/bin/env bash

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

#TODO: Right usage
function usage {
  echo "$0: TODO"
}

# Note that we use "$@" to let each command-line parameter expand to a
# separate word. The quotes around "$@" are essential!
# We need TEMP as the 'eval set --' would nuke the return value of getopt.
TEMP=$(getopt -o 'n:i:o:a:' --long 'name:,iface:,obj:,address:' -n 'cb-start' -- "$@")

if [ $? -ne 0 ]; then
	echo 'Terminating...' >&2
	exit 1
fi

# Note the quotes around "$TEMP": they are essential!
eval set -- "$TEMP"
unset TEMP

while true; do
	case "$1" in
		'-n'|'--name')
      NAME="$2"
			shift 2
			continue
		;;
		'-i'|'--iface')
      IFACE="$2"
			shift 2
			continue
		;;
		'-o'|'--obj')
      OBJ="$2"
			shift 2
			continue
		;;
		'-a'|'--address')
      ADDR="$2"
			shift 2
			continue
		;;
		'--')
			shift
			break
		;;
		*)
			echo "BUG!!! Unknown internal arg $1" >&2
			exit 1
		;;
	esac
done

# Required arguments
if [ -z $NAME ]; then
  echo "Expected name (-n or --name)"
  usage
  exit 1 
fi

if [ -z $IFACE ] ; then
  echo "Expected interface (-i or --iface)"
  usage
  exit 1
fi

if  [ -z $OBJ ]; then 
  echo "Expected obj path (-o or --obj)"
  usage
  exit 1
fi

AGENT="$SCRIPTDIR/cb_agent"
if [ ! -f "$AGENT" ]; then
  echo "Could not find 'cb_agent' executable: $AGENT"
  exit 1
fi

# Start SF
"$@" &

# Start agent
$AGENT $NAME $IFACE $OBJ $ADDR > /dev/null
