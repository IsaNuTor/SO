MPOINT="./mount-point"

fusermount -u mount-point
rmdir $MPOINT/
mkdir $MPOINT/
make 
./fs-fuse -t 2097152 -a virtual-disk -f '-s -d mount-point'
#./fs-fuse -a virtual-disk -m -f '-d -s mount-point'


#Modificacion Práctica 2 Symlink
cd $MPOINT
echo "hola" > hola.txt
ls
ln -s hola.txt sym.txt
if ! diff hola.txt sym.txt
then
	echo "Son diferentes"
	exit 1
fi

echo "Todo correcto!"

