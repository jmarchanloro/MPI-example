# MPI-example

This code applies a sobel mask on the photo.dat. The MPI program creates a N children. Each childen reads a bunch of data and make the conversion. 

How to compile and to execute:
>mpicc -o mpi-example mpi-example.c -lX11
>mpirun -n 1 ./mpi-example
