
echo "enter the name of application as argument and approximate duration of execution"

let N=$2 

echo $N

for i in {1..60}
do
	grep bytes /proc/$(pidof $1)/io
	sleep 1
done

