#include "procesosMemoria.h"
typedef int (*t_comando)(int argc, char *argv[]);

void borrarContenidoTLB(){
	pthread_mutex_lock(mutex_TLB);
	queue_clean(TLB);
	pthread_mutex_unlock(mutex_TLB);
}

void enviarASwap(PaginaFrame * pagina, char * key){
	Marco * marco = malloc(sizeof(Marco));
	marco = list_get(tablaDeMarcos, pagina->marco);
	if (pagina->seModifico) {
		char* msjAEnviar = string_new();
		string_append(&msjAEnviar, "2|");
		string_append(&msjAEnviar, key);
		string_append(&msjAEnviar, "|");
		string_append(&msjAEnviar, pagina->pagina);
		string_append(&msjAEnviar, "|");
		string_append(&msjAEnviar, marco->contenidoMarco);
		enviar_string(socketClienteSwap->fd, msjAEnviar);
		char* respuesta = recibir_y_deserializar_mensaje(socketClienteSwap->fd);
	}
	marco->bitOcupado = 0;
	marco->contenidoMarco = "\0";
}

void borrarProceso(char * key, t_queue * tablaDePaginas) {
	int i = 0;
	PaginaFrame * pagina = malloc(sizeof(PaginaFrame));
	while (i < queue_size(tablaDePaginas)) {
		pagina = queue_pop(tablaDePaginas);
		enviarASwap(pagina, key);
	}
}

void borrarPaginasDeLosProcesos(){
	pthread_mutex_lock(mutex_tablaDePaginas);
	dictionary_iterator(tablaProcesos, (void*) borrarProceso);
	pthread_mutex_unlock(mutex_tablaDePaginas);
}

void logContenidoMarcos() {
	pthread_mutex_lock(mutex_tablaDeMarcos);
	Marco * marco = malloc(sizeof(Marco));
	int i;
	for(i = 0; i < list_size(tablaDeMarcos); i++){
		marco = list_get(tablaDeMarcos, i);
		char* info_log = string_new();
		info_log = string_itoa(i);
		string_append(&info_log, "        |  ");
		string_append(&info_log, marco->contenidoMarco);
		string_append(&info_log, "  |");
		log_info(logMemoriaOculto, info_log);
		free(info_log);
	}
	pthread_mutex_unlock(mutex_tablaDeMarcos);
}



void limpiar_TLB() {
	log_info(logMemoriaOculto, "Senial 'limpiar_TLB' recibida, comenzando ejecucion\n");
	borrarContenidoTLB();
	log_info(logMemoriaOculto, "Tratamiento de senial 'limpiar_TLB' terminada\n");
}

void limpiar_Memoria() {
	log_info(logMemoriaOculto, "Senial 'limpiar_memoria' recibida, comenzando ejecucion\n");
	borrarPaginasDeLosProcesos();
	borrarContenidoTLB();
	log_info(logMemoriaOculto, "Tratamiento de senial 'limpiar_memoria' terminada\n");
}

void volcar_memoria() {
	pid_t childPID;
	childPID = fork();
	if (childPID >= 0) // fork was successful
			{
		if (childPID == 0) { // child process
			log_info(logMemoriaOculto, "Senial 'volcar_memoria' recibida, comenzando ejecucion\n");
			logContenidoMarcos();
			log_info(logMemoriaOculto, "Tratamiento de senial 'volcar_memoria' terminada\n");
			cerrarMemoria();
			exit(EXIT_SUCCESS);
		} else { //Parent process
			usleep(500000);
			borrarPaginasDeLosProcesos();
			borrarContenidoTLB();
		}
	} else // fork failed
	{
		printf("\n Fork failed, quitting!!!!!!\n");
	}
}


void crearHiloLimpiarTLB(){
	int32_t Jth;
	pthread_t thread;
	Jth = pthread_create(&thread, NULL, limpiar_TLB, NULL);
	if (Jth != 0) {
		log_info(logMemoriaOculto, "Error al crear el hilo para limpiar la TLB. Devolvio: %d\n", Jth);
	} else {
		log_info(logMemoriaOculto, "Se creo exitosamente el hilo para limpiar la TLB\n");
	}
}

void crearHiloLimpiarMemoria(){
	int32_t Jth;
	pthread_t thread;
	Jth = pthread_create(&thread, NULL, limpiar_Memoria, NULL);
	if (Jth != 0) {
		log_info(logMemoriaOculto, "Error al crear el hilo para limpiar la memoria. Devolvio: %d\n", Jth);
	} else {
		log_info(logMemoriaOculto, "Se creo exitosamente el hilo para limpiar la memoria\n");
	}
}

void crearHiloVolcarMemoria(){
	int32_t Jth;
	pthread_t thread;
	Jth = pthread_create(&thread, NULL, volcar_memoria, NULL);
	if (Jth != 0) {
		log_info(logMemoriaOculto, "Error al crear el hilo para volcar la memoria. Devolvio: %d\n", Jth);
	} else {
		log_info(logMemoriaOculto, "Se creo exitosamente el hilo para volcar la memoria\n");
	}
}



