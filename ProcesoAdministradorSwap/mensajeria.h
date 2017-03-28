/*
 * mensajeria.h
 *
 *  Created on: 11/10/2015
 *      Author: utnso
 */

#ifndef MENSAJERIA_H_
#define MENSAJERIA_H_
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>

typedef struct{
	int32_t PID;
	int32_t cantidadPaginasAUsar;
}mensaje_peticion_inicio;

typedef struct{
	int32_t PID;
	int32_t pagina;
	char* texto;
}mensaje_peticion_escritura;

typedef struct{
	int32_t PID;
	int32_t pagina;
}mensaje_peticion_lectura;

typedef struct{
	int32_t PID;
}mensaje_peticion_finalizar;


mensaje_peticion_inicio* recibir_mensaje_peticion(char*);
mensaje_peticion_escritura* recibir_mensaje_escritura(char*);
mensaje_peticion_lectura* recibir_mensaje_lectura(char*);
mensaje_peticion_finalizar* recibir_mensaje_finalizar(char*);

#endif /* MENSAJERIA_H_ */
