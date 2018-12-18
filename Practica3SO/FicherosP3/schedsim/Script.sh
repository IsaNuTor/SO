#! /bin/bash

#! Nombre del fichero de ejemplo que se desea simular

	echo "¿Que archivo quiere simular?"
	read FILE			#! lee el archivo

#! Numero maximo de CPUS que se desean simular

NUMCPU=0
FIN=0
#! Mientras no llegue al final
while [ $FIN -ne 1 ] ; do
	echo "¿Cuantas CPUs quiere simular? Maximo 8"
	read NUMCPU
	
	if [ $NUMCPU -le 0 ] || [ $NUMCPU -ge 9 ] ; then

		echo "El numero de CPUs no el valido"
	else
		let FIN=1
	fi
done

if [ ! -d "resultados" ] 
then
    #! Si no existe, crea un directorio resultados
	mkdir resultados	
fi

<<COMMENT
for nameSched in RR FCFS SJF EXPRIO 
do
    ./schedsim -i $FILE -n NUMCPU -s nameSched
   
        mv CPU_0.log resultados/nameSched-CPU-0.log
        cd ..
        cd gantt-gplot
        ./generate_gantt_chart ../schedsim/resultados/nameSched-CPU-0.log

        cd ../schedsim
   
done
COMMENT

#! RR
echo "Realizando simulacion RR"
./schedsim -i $FILE -n $NUMCPU -s RR

mv CPU_0.log resultados/RR_CPU_0.log

cd ..
cd gantt-gplot

./generate_gantt_chart ../schedsim/resultados/RR_CPU_0.log

cd ../schedsim

#! SJF
echo "Realizando simulacion SJF"
./schedsim -i $FILE -n $NUMCPU -s SJF

mv CPU_0.log resultados/SJF_CPU_0.log

cd ..
cd gantt-gplot

./generate_gantt_chart ../schedsim/resultados/SJF_CPU_0.log

cd ../schedsim

#! FCFS
echo "Realizando simulacion FCFS"
./schedsim -i $FILE -n $NUMCPU -s FCFS

mv CPU_0.log resultados/FCFS_CPU_0.log

cd ..
cd gantt-gplot

./generate_gantt_chart ../schedsim/resultados/FCFS_CPU_0.log

cd ../schedsim

#! EXPRIO
echo "Realizando simulacion EXPRIO"
./schedsim -i $FILE -n $NUMCPU -s EXPRIO

mv CPU_0.log resultados/EXPRIO_CPU_0.log

cd ..
cd gantt-gplot

./generate_gantt_chart ../schedsim/resultados/EXPRIO_CPU_0.log

cd ../schedsim

#! multiRR
echo "Realizando simulacion MULTIRR"
./schedsim -i $FILE -n $NUMCPU -s MULTIRR -p

mv CPU_0.log resultados/MULTIRR_CPU_0.log

cd ..
cd gantt-gplot

./generate_gantt_chart ../schedsim/resultados/MULTIRR_CPU_0.log

cd ../schedsim

