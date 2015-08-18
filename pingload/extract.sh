#!/bin/bash



if [ $# -eq 0 ]
then
	echo "Usage: extract [-s|--sort|-a|--all|-t|--time|-i|--inform] LOGFILE";
	exit;
fi

for arg in "$@"
do
	case $arg in
		-a|--all)
			ALL=1;
			;;
		-t|--time)
			TIME=1;
			;;
		-i|--intform)
			INTFORM=1;
			;;
		-s|--sort)
			SORT=1;
			;;
		*)
			LOGFILE=$arg;
			;;
	esac
done

#printf "Logfile: %s\n" $LOGFILE;

cat $LOGFILE | gawk '
#BEGIN{print "---BEGIN---"}
{
if($1 !~ /rtt|---|PING/ && $2 != "packets" && $3 ~ /bytes|answer/)
	{
		time=$1;
		seq=$6;
		ping=$8;
		if($time!="" && ping=="") ping="time=0";
		#print time"\t"ping
		print seq"\t"time"\t"ping
	}
}
#END{print "---END---"}'|\
sed -r 's/\[//g; s/\]//g; s/icmp_seq=//g; s/time=//g;' | sort -n |\
#gawk -v all=$ALL -v time=$TIME -v intform=$INTFORM '
gawk -v all=$ALL -v time=$TIME -v intform=$INTFORM -v sorted=$SORT '
{
	pingtimes[$1] = $2;
	pingvalues[$1] = $3;
}
END {
	if ( sorted )
	{
		asort(pingvalues);
		for ( i = 1; i in pingvalues; i++){
			printf "%-5.1f\n", pingvalues[i];
		}
	}
	else 
	{
		basetime = pingtimes[1];
		for (i = 1; i in pingvalues; i++){
			if( all )
				printf "%-5d\t%-10.5f\t%-5.1f\n", i, (pingtimes[i] - basetime), pingvalues[i];	#Print icmp_sqe, date and ping.
			else if( time )
				printf "%-10.5f\t%-5.1f\n", (pingtimes[i] - basetime), pingvalues[i];		#Print date and ping.
			else if( intform )
				printf "%d\n", pingvalues[i];							#Print ping only in integers.
			else
				printf "%-5.1f\n", pingvalues[i];						#Print ping only.
		}
	}
}
'