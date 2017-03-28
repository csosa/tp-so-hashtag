#ifndef FUNCIONESSWAP_H_
#define FUNCIONESSWAP_H_
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>

typedef struct{
	int32_t puerto_escucha;
	char* nombre_swap;
	u_int32_t cantidad_paginas;
	u_int32_t tamanio_pagina;
	u_int32_t retardo_compactacion;
	u_int32_t retardo_swap;
}swap;

typedef struct{
	u_int32_t cantidadPaginas;
	char* dato;
	u_int32_t pid;
}paquete;

typedef struct{
	u_int32_t pid;
	u_int32_t posicion;
	int32_t pagina;
	u_int32_t tamanio;
}proceso;

typedef struct{
	char* dato;
	int32_t pagina;
	u_int32_t pid;
}dato;

typedef struct{
	u_int32_t pid;
	int32_t cantidad;
}datoDeLectura;

typedef struct{
	u_int32_t comienzo;
	int32_t cantPaginas;
}estructura_vacia;

swap* archINICIO;
FILE* particionSwap;
FILE* nuevaParticion;
u_int32_t cantidaTotalDePaginasDisponibles;
t_log* logSwap;
t_list* procesosEnSwap;
t_list* espaciosLibres;
t_list* paginasLeidasPorCadaProceso;
t_list* paginasEscritasPorCadaProceso;
char unico;

void compactar();
swap* levantarConfig(char* path);
void iniciar_comparticion_swap();
int32_t crearHiloSwap(pthread_t* , paquete* , t_log* );



#endif /* FUNCIONESSWAP_H_ */
