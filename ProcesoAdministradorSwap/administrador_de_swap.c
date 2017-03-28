#include "administrador_de_swap.h"
#include "mensajeria.h"
#include "socket_lib.h"
#include "funcionesSwap.h"
#define AZUL  "\x1b[32m"

#define iniciar 1
#define escritura 2
#define lectura 3
#define darDeBaja 4
#define finalizar 5
#define caer 6

void iniciar_configuracion() {
	printf(AZUL
	"             __^__                                      __^__\n"
	"            ( ___ )------------------------------------( ___ )   \n"
	"             | / |                                      | / |  \n"
	"             | / |      Bienvenidos al proceso Swap     | / |   \n"
	"             |___|                                      |___| \n"
	"            (_____)------------------------------------(_____)\n");

	/* Levanto el archivo de configuración*/
	archINICIO = levantarConfig("swap.config");

	/* Me levanto como servidor para escuchar al Proceso Memoria*/
	socketServer = create_server_socket(archINICIO->puerto_escucha);
}

void manage_Swap_shutdown() {
	printf("Desea bajar la ejecucion del proceso Swap? Si/No ");
	char respuesta[3];
	respuesta[2] = '\0';
	scanf("%s", respuesta);
	while (strcasecmp(respuesta, "Si") != 0 && strcasecmp(respuesta, "No") != 0) {
		printf(
				"La opcion ingresada no es correcta. Por favor indique si desea dar de baja el Swap.\n");
		scanf("%s", respuesta);
	}
	if (strcasecmp(respuesta, "Si") == 0) {
		exit(EXIT_SUCCESS);
	} else {
		printf("Mejor, no es recomendable bajar el Swap\n");
	}
	free(respuesta);
}

int main() {
	if (signal(SIGINT, manage_Swap_shutdown) == SIG_ERR) {
		log_error(logSwap, "FS [ERROR] Error con la señal SIGINT");
	}

	/*Creo logs para el hilo */
	char* archLogSwap = "funcionesSwap.c";
	logSwap = log_create("logFileSwap.log", archLogSwap, 0, LOG_LEVEL_INFO);
	log_info(logSwap,
			"\n             __^__                                      __^__\n"
					"            ( ___ )------------------------------------( ___ )   \n"
					"             | / |                                      | / |  \n"
					"             | / |      Bienvenidos al proceso Swap     | / |   \n"
					"             |___|                                      |___| \n"
					"            (_____)------------------------------------(_____)\n");
	iniciar_configuracion();

	/*Creo tabla para administrar procesos*/
	procesosEnSwap = list_create();
	paginasLeidasPorCadaProceso = list_create();
	espaciosLibres = list_create();
	paginasEscritasPorCadaProceso = list_create();

	cantidaTotalDePaginasDisponibles = archINICIO->cantidad_paginas;
	iniciar_particion();
	particionSwap = fopen(archINICIO->nombre_swap, "r+");
	iniciar_comparticion_swap();
	fclose(particionSwap);

	printf("Esperando a la Memoria...\n");

	listen_connections(socketServer);
	int32_t accept = 1;
	sock_t* socketMemoria = accept_connection(socketServer);
	printf("Conectado a la memoria :)\n");
	while (accept) {
		int32_t codigoOperacion;

		char* mensaje = recibir_y_deserializar_mensaje(socketMemoria->fd);
		char** respuesta = string_split(mensaje, "|");
		codigoOperacion = atoi(respuesta[0]);
		if (1 <= 0) {
			printf("Se cayó la memoria.\n");
			log_error(logSwap, "[Swap] Error al recibir código de operación\n");
		} else {
			if (codigoOperacion != escritura && codigoOperacion != lectura
					&& codigoOperacion != darDeBaja
					&& codigoOperacion != iniciar
					&& codigoOperacion != finalizar && codigoOperacion != caer) {
				log_error(logSwap,
						"[Swap] Error al recibir el proceso para escribir\n");
				int32_t code = 1;
				enviar_string(socketMemoria->fd, string_itoa(code));
			} else {
				if (codigoOperacion == escritura) {
					particionSwap = fopen(archINICIO->nombre_swap, "r+");
					mensaje_peticion_escritura* peticionEscritura =
							recibir_mensaje_escritura(mensaje);
					se_escribe_pagina(peticionEscritura, logSwap);
					fclose(particionSwap);
					int32_t code = 0;

					enviar_string(socketMemoria->fd, string_itoa(code));
				} else {
					if (codigoOperacion == lectura) {
						particionSwap = fopen(archINICIO->nombre_swap, "r+");
						mensaje_peticion_lectura* peticionLectura =
								recibir_mensaje_lectura(mensaje);
						char* contenido = leer_pagina(peticionLectura, logSwap);
						fclose(particionSwap);
						enviar_string(socketMemoria->fd, contenido);
					} else {
						if (codigoOperacion == darDeBaja) {
						} else {
							if (codigoOperacion == iniciar) {
								mensaje_peticion_inicio* peticionInicio =
										recibir_mensaje_peticion(mensaje);
								particionSwap = fopen(archINICIO->nombre_swap,
										"r+");
								if (cantidaTotalDePaginasDisponibles
										>= peticionInicio->cantidadPaginasAUsar) {

									if (!hay_espacio_contiguo(
											peticionInicio->cantidadPaginasAUsar,
											peticionInicio->PID, logSwap)) {
										fclose(particionSwap);
										log_info(logSwap,
												"Se inicia la COMPACTACION por fragmentacion externa\n");

										nuevaParticion = fopen("prueba", "wt");
										rellenar_archivo_con_barra_cero_paralelo(
												nuevaParticion);
										fclose(nuevaParticion);
										particionSwap = fopen(
												archINICIO->nombre_swap, "r+");
										compactar();
										usleep(
												archINICIO->retardo_compactacion
														* 100);
										log_info(logSwap,
												"Se finaliza la COMPACTACION por fragmentacion externa\n");
										int32_t code = 0;
										enviar_string(socketMemoria->fd,
												string_itoa(code));
									} else {
										int32_t code = 0;
										enviar_string(socketMemoria->fd,
												string_itoa(code));
									}
								} else {
									log_info(logSwap,
											"El proceso %d ha sido rechazado por falta de espacio\n",
											peticionInicio->PID);
									int32_t code = 1;
									enviar_string(socketMemoria->fd,
											string_itoa(code));
								}
							} else {
								if (codigoOperacion == finalizar) {
									mensaje_peticion_finalizar* peticionFinalizar =
											recibir_mensaje_finalizar(mensaje);
									particionSwap = fopen(
											archINICIO->nombre_swap, "r+");
									dar_de_baja(peticionFinalizar->PID,
											logSwap);
									fclose(particionSwap);
									int32_t code = 0;
									enviar_string(socketMemoria->fd,
											string_itoa(code));
								} else {
									if (codigoOperacion == caer) {
										printf("Se cayó la memoria :( \n");
										return EXIT_FAILURE;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	log_error(logSwap, "[SW] Se cierra la conexion\n");
	free(procesosEnSwap);
	free(paginasLeidasPorCadaProceso);
	free(espaciosLibres);
	log_destroy(logSwap);
	printf("     +-------------------------+ \n"
			"    ¦    Saludos, Hashtag     ¦\n"
			"    +-------------------------+\n");
	return 0;
}
