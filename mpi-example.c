/* Pract2  RAP 09/10    Javier Ayllon*/

#include <openmpi/mpi.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <X11/Xlib.h> 
#include <assert.h>   
#include <unistd.h>   
#define NIL (0)
#define N 4
#define FILENAME "./photo.dat"
#define FILAS 400
#define COLUMNAS 400   
#define TAMANYO FILAS*COLUMNAS    

/*Variables Globales */

XColor colorX;
Colormap mapacolor;
char cadenaColor[]="#000000";
Display *dpy;
Window w;
GC gc;

void convertir_a_grises(unsigned char *buffer, int bufsize);
void conseguir_lineas(unsigned char *buffer, unsigned char *buf_recv_line_sup, unsigned char *buf_recv_line_inf, int rank, int);

/*Funciones auxiliares */

void initX() {

      dpy = XOpenDisplay(NIL);
      assert(dpy);

      int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
      int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

      w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     400, 400, 0, blackColor, blackColor);
      XSelectInput(dpy, w, StructureNotifyMask);
      XMapWindow(dpy, w);
      gc = XCreateGC(dpy, w, 0, NIL);
      XSetForeground(dpy, gc, whiteColor);
      for(;;) {
            XEvent e;
            XNextEvent(dpy, &e);
            if (e.type == MapNotify)
                  break;
      }


      mapacolor = DefaultColormap(dpy, 0);

}

void dibujaPunto(int x,int y, int r, int g, int b) {

        sprintf(cadenaColor,"#%.2X%.2X%.2X",r,g,b);
        XParseColor(dpy, mapacolor, cadenaColor, &colorX);
        XAllocColor(dpy, mapacolor, &colorX);
        XSetForeground(dpy, gc, colorX.pixel);
        XDrawPoint(dpy, w, gc,x,y);
        XFlush(dpy);

}



/* Programa principal */

int main (int argc, char *argv[]) {

  int rank,size;
  MPI_Comm commPadre;
  int tag;
  MPI_Status status;
  int buf[5];

  MPI_File fh;
  int i,j, rankHijo, indice, mi_bloque;
  int err[N], A, B;                       /* Codigos de error para MPI_Comm_spawn*/
  int bufsize = (FILAS*COLUMNAS) / N;     /* Numero de puntos de la imagen para cada proceso          */
  unsigned char *buffer;                  /* Buffer para lectura (de bufsize puntos de la imagen)     */
  unsigned char buf_recv;                 /* Buffer para la recepcion de un punto (1 punto = 3 uchar) */
  unsigned char *buf_recv_line_sup;       /* Buffer para la recepcion de una fila                       */
  unsigned char *buf_recv_line_inf;       /* Buffer para la recepcion de una fila                       */
  unsigned char usupi, usup, usupd, uizq, /* Vecinos de un punto (i,j)*/
    uder, uinfi, uinf, uinfd;

  buf_recv_line_sup  = (unsigned char *) malloc(COLUMNAS*sizeof(unsigned char)*3);
  buf_recv_line_inf  = (unsigned char *) malloc(COLUMNAS*sizeof(unsigned char)*3);

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_get_parent( &commPadre );


  if ( (commPadre==MPI_COMM_NULL)
        && (rank==0) )  {

	initX();
	

	/* Codigo del maestro */

	/*En algun momento dibujamos puntos en la ventana algo como
	dibujaPunto(x,y,r,g,b);  */

	MPI_Comm_spawn("./pract2", MPI_ARGV_NULL, N, MPI_INFO_NULL,
		0, MPI_COMM_WORLD, &commPadre, err);


        for(i=0; i<FILAS*COLUMNAS; i++){
	  MPI_Recv(&buf_recv, 1, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, commPadre, &status);
	  rankHijo = status.MPI_SOURCE;
	  tag      = status.MPI_TAG;      /* tag es el numero del punto dentro del bloque del proceso*/
	  
	  
	  dibujaPunto(((bufsize*rankHijo)+tag)%FILAS, ((bufsize*rankHijo)+tag)/FILAS,buf_recv,buf_recv,buf_recv);
	} 
	
        sleep(6);
  }

 	
  else {
    /* Codigo de todos los trabajadores */
    /* El archivo sobre el que debemos trabajar es foto.dat */
 
    //Leer el trozo correspondiente de la imagen
    MPI_File_open(MPI_COMM_WORLD, FILENAME , MPI_MODE_RDONLY,MPI_INFO_NULL, &fh);
    buffer  = (unsigned char *) malloc(bufsize*sizeof(unsigned char)*3);
    MPI_File_set_view(fh, rank*bufsize*sizeof(unsigned char)*3, MPI_UNSIGNED_CHAR, MPI_UNSIGNED_CHAR, "native", MPI_INFO_NULL);
    MPI_File_read(fh, buffer, bufsize*sizeof(unsigned char)*3, MPI_UNSIGNED_CHAR, &status);
 

    convertir_a_grises(buffer, bufsize);
    conseguir_lineas(buffer, buf_recv_line_sup, buf_recv_line_inf, rank, bufsize);

    // Aplicar mascara de 3x3
    // Para aplicar cualquier mascara la siguiente suposicion:
    // Cada proceso lee al menos un numero COLUMNAS de puntos. Por
    // ejemplo, para 400x400 como maximo habra 400 procesos y cada
    // uno se encarga de leer 400 puntos (una fila) por lo que 
    // no intervendra mas de un proceso en la lectura de una linea


    // indice     es el indice absoluto de un punto de la imagen
    // mi_bloque  es el punto absoluto donde empieza mi bloque

    // Notar que indice avanza punto por punto. Cuando es 
    // necesario se ajusta al elemento correspondiente del buffer multiplicando por 3
    mi_bloque = rank*bufsize;
    int aux;

    // A cada punto se le aplica la mascara
    for(j=0; j<bufsize; j++){
      indice = mi_bloque + j;
      
      // Se omite el borde de la imagen
      if((indice%FILAS != 0     &&          // No es la primera columna
      	  (indice+1)%FILAS != 0 &&          // No es la ultima  columna
      	  indice > COLUMNAS     &&          // No es la fila superior
      	  indice < TAMANYO - FILAS )){      // No es la fila inferior
	
    	if(mi_bloque<=indice-COLUMNAS-1){
    	  // La fila superior esta en mi bloque almacenado en buffer
    	  usupi = buffer[3*(j-COLUMNAS-1)];
    	  usup  = buffer[3*(j-COLUMNAS)];
    	  usupd = buffer[3*(j-COLUMNAS + 1)];
    	}
    	else{
    	  // La fila superior esta en la linea adicional recibida
    	  usupi = buf_recv_line_sup[3*(j%COLUMNAS-1)];
    	  usup  = buf_recv_line_sup[3*(j%COLUMNAS)];
    	  usupd = buf_recv_line_sup[3*(j%COLUMNAS+1)];
    	}

    	uizq  = buffer[3*(j-1)];
    	uder  = buffer[3*(j+1)];
	
    	if(indice+COLUMNAS+1 <mi_bloque+bufsize){
    	  // La fila inferior esta en mi bloque almacenado en buffer
    	  uinfi = buffer[3*(j+COLUMNAS-1)];
    	  uinf  = buffer[3*(j+COLUMNAS)];
    	  uinfd = buffer[3*(j+COLUMNAS+1)];
    	}
    	else{
    	  // La fila inferior esta en la linea adicional recibida
    	  uinfi = buf_recv_line_inf[3*(j%COLUMNAS-1)];
    	  uinf  = buf_recv_line_inf[3*(j%COLUMNAS)];
    	  uinfd = buf_recv_line_inf[3*(j%COLUMNAS +1)];
    	}

    	//Sobel
    	A = (usupi + (usup+usup) + usupd - uinfi - (uinf+uinf) - uinfd)/4;
    	B = (usupd + (uder+uder) + uinfd - usupi - (uizq+uizq) - uinfi)/4;
    	aux = sqrt(A*A + B*B);


      }  // Fin if

      MPI_Send(&aux, 1, MPI_UNSIGNED_CHAR, 0, j, commPadre);
    }
      


    MPI_File_close(&fh);
    
  }

  MPI_Finalize();
  return 0;
}


void convertir_a_grises(unsigned char *buffer, int bufsize)
{
  int j;
  for(j=0; j<bufsize; j++){
    buffer[j*3 + 0] = buffer[j*3 + 0]*0.299 +
      buffer[j*3 + 1]*0.587 +
      buffer[j*3 + 2]*0.114;
  }

}


/* Envia las lineas que cada proceso necesita de el y recibe las necesarias del resto de procesos*/
void conseguir_lineas(unsigned char *buffer, unsigned char *buf_recv_line_sup, 
		      unsigned char *buf_recv_line_inf, int rank, int bufsize)
{
  MPI_Request request1, request2, request3, request4;
  MPI_Status status;

    /* ¿Que lineas adicionales necesita cada proceso? :
       -- Sea el proceso x, donde 0<x<N-1, necesita la linea superior del proceso x+1
          y la linea inferior del proceso x-1
       -- El proceso 0 necesita la linea superior del proceso 1
       -- El proceso N-1 necesita la linea inferiro del proceso (N-1)-1?
    */

    //Enviar mi fila superior
    if(rank > 0)
      MPI_Isend(&(buffer[0]), COLUMNAS*3, MPI_UNSIGNED_CHAR, rank-1, 0, MPI_COMM_WORLD, &request1);
 
    
    //Enviar mi fila inferior
    if(rank < N-1 )
      MPI_Isend(&buffer[3*(bufsize - COLUMNAS-1)], COLUMNAS*3, MPI_UNSIGNED_CHAR, rank+1, 0, MPI_COMM_WORLD, &request3);//Tag no se usa


    // El proceso necesita la fila inmediatamente superior (== fila inferior del proceso rank-1)
    if ( rank > 0 ) 
      //buf_recv_line_sup  = (unsigned char *) malloc(COLUMNAS*sizeof(unsigned char)*3);
      MPI_Irecv(&(buf_recv_line_sup[0]), COLUMNAS*3, MPI_UNSIGNED_CHAR, rank-1, MPI_ANY_TAG, MPI_COMM_WORLD, &request4);

    // El proceso necestia la fila inmediatamente inferior (== fila superior del proceso rank + 1)
    if ( rank < N-1  )
      MPI_Irecv(&(buf_recv_line_inf[0]), COLUMNAS*3, MPI_UNSIGNED_CHAR, rank+1, MPI_ANY_TAG, MPI_COMM_WORLD, &request2);
      


    if(rank > 0){
      MPI_Wait(&request1, &status);
      MPI_Wait(&request4, &status);
    
    }
    if(rank < N-1){
      MPI_Wait(&request2, &status);
      MPI_Wait(&request3, &status);
    }
    
}
