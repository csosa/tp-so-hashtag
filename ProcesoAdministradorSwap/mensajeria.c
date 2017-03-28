/*
 * mensajeria.c
 *
 *  Created on: 11/10/2015
 *      Author: MeliMacko
 */
#include "mensajeria.h"

mensaje_peticion_inicio* recibir_mensaje_peticion(char* mensaje) {
	char** respuesta = string_split(mensaje, "|");
	mensaje_peticion_inicio* peticion = malloc(sizeof(mensaje_peticion_inicio));
	peticion->PID = atoi(respuesta[1]);
	peticion->cantidadPaginasAUsar = atoi(respuesta[2]);
	free(respuesta);
	return peticion;
}

mensaje_peticion_escritura* recibir_mensaje_escritura(char* mensaje) {
	char** respuesta = string_split(mensaje, "|");
	mensaje_peticion_escritura* peticion = malloc(
			sizeof(mensaje_peticion_escritura));
	peticion->PID = atoi(respuesta[1]);
	peticion->pagina = atoi(respuesta[2]);
	peticion->texto = respuesta[3];
	free(respuesta);
	return peticion;
}

mensaje_peticion_lectura* recibir_mensaje_lectura(char* mensaje) {
	char** respuesta;
	respuesta = string_split(mensaje, "|");
	mensaje_peticion_lectura* peticion = malloc(
			sizeof(mensaje_peticion_lectura));
	peticion->PID = atoi(respuesta[1]);
	peticion->pagina = atoi(respuesta[2]);
	free(respuesta);

	return peticion;
}

mensaje_peticion_finalizar* recibir_mensaje_finalizar(char* mensaje) {
	char** respuesta = string_split(mensaje, "|");
	mensaje_peticion_finalizar* peticion = malloc(
			sizeof(mensaje_peticion_finalizar));
	peticion->PID = atoi(respuesta[1]);
	free(respuesta);
	return peticion;
}
