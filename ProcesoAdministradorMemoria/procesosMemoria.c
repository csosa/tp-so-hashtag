#include "procesosMemoria.h"

void borrarTablaDePaginas(char * key, t_queue * tablaDePaginas){
	int j = queue_size(tablaDePaginas);
	int i = 0;
	while(i < j){
		PaginaFrame * pagina = malloc(sizeof(PaginaFrame));
		pagina = queue_pop(tablaDePaginas);
		free(pagina);
		i++;
	}
	free(tablaDePaginas);
}

void borrarMarco(Marco * marco){
	free(marco);
}



pthread_mutex_t * dar_mutex(){
    pthread_mutex_t * a_devolver = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(a_devolver, NULL);
    pthread_mutex_unlock(a_devolver);
    return a_devolver;
}

void destruir_mutex(pthread_mutex_t * mutex){
    pthread_mutex_destroy(mutex);
    free(mutex);
}

void cerrarMemoria(){
	dictionary_iterator(tablaProcesos, (void*) borrarTablaDePaginas);
	dictionary_destroy(tablaProcesos);
	list_destroy_and_destroy_elements(tablaDeMarcos,(void*) borrarMarco);
	dictionary_destroy(estadisticasProcesos);
	queue_destroy(TLB);
	destruir_mutex(mutex_TLB);
	destruir_mutex(mutex_tablaDeMarcos);
	destruir_mutex(mutex_tablaDePaginas);
	log_destroy(logMemoriaOculto);
	log_destroy(logMemoriaPantalla);
	free(configuracion);
	free(socketServerCPU);
	free(socketClienteSwap);
}

void crear_estructura_config(char* path){
    t_config * archConfig = config_create(path);
    configuracion = malloc(sizeof(memoria));
    configuracion->puerto_swap = config_get_int_value(archConfig, "PUERTO_SWAP");
    configuracion->ip_swap = config_get_string_value(archConfig, "IP_SWAP");
    configuracion->puerto_escucha = config_get_int_value(archConfig, "PUERTO_ESCUCHA");
    configuracion->maximo_marcos_por_proceso = config_get_int_value(archConfig, "MAXIMO_MARCOS_POR_PROCESO");
    configuracion->cantidad_marcos = config_get_int_value(archConfig, "CANTIDAD_MARCOS");
    configuracion->tamanio_marcos = config_get_int_value(archConfig, "TAMANIO_MARCO");
    configuracion->entradas_tlb = config_get_int_value(archConfig, "ENTRADAS_TLB");
    configuracion->tlb_habilitada = config_get_string_value(archConfig, "TLB_HABILITADA");
    configuracion->algoritmoDeReemplazo = config_get_string_value(archConfig, "ALGORITMO_REEMPLAZO");
    configuracion->retardo_memoria = config_get_int_value(archConfig, "RETARDO_MEMORIA");
    free(archConfig);
}

PaginaFrame * eliminarPorFifo(t_queue * tablaDePaginas){
	PaginaFrame * paginaAEliminar = malloc(sizeof(PaginaFrame));
	paginaAEliminar = queue_pop(tablaDePaginas);
	return paginaAEliminar;
}

PaginaFrame * eliminarPorLRS(t_queue * tablaDePaginas){
	PaginaFrame * paginaAEliminar = malloc(sizeof(PaginaFrame));
	paginaAEliminar = queue_pop(tablaDePaginas);
	return paginaAEliminar;
}

PaginaFrame * eliminarporClockM(t_queue * tablaDePaginas){
	PaginaFrame * paginaAEliminar = malloc(sizeof(PaginaFrame));
	PaginaFrame * paginaCandidata = malloc(sizeof(PaginaFrame));
	int i = 0;
	bool encontroCandidata = false;
	//recorro la lista buscando una candidata con bit de uso y bit de modificado en 0
	while(i < queue_size(tablaDePaginas) && !encontroCandidata){
		paginaCandidata = queue_pop(tablaDePaginas);
		if(paginaCandidata->bitUso == 0 && paginaCandidata->seModifico == 0){
			paginaAEliminar = paginaCandidata;
			encontroCandidata = true;
		}
		else {
			queue_push(tablaDePaginas, paginaCandidata);
		}
		i++;
	}
	i = 0;
	//si no encontre candidata, aplico el paso dos del algoritmo clock-m
	if(!encontroCandidata){
		while(i < queue_size(tablaDePaginas) && !encontroCandidata){
			paginaCandidata = queue_pop(tablaDePaginas);
			if(paginaCandidata->bitUso == 0 && paginaCandidata->seModifico == 1){
				paginaAEliminar = paginaCandidata;
				encontroCandidata = true;
			}
			else{
				paginaCandidata->bitUso = 0;
				queue_push(tablaDePaginas, paginaCandidata);
			}
			i++;
		}
	}
	i = 0;
	//si no encontre candidata, repito el paso 1
	if(!encontroCandidata){
		while(i < queue_size(tablaDePaginas) && !encontroCandidata){
			paginaCandidata = queue_pop(tablaDePaginas);
			if(paginaCandidata->bitUso == 0 && paginaCandidata->seModifico == 0){
				paginaAEliminar = paginaCandidata;
				encontroCandidata = true;
			}
			else {
				queue_push(tablaDePaginas, paginaCandidata);
			}
			i++;
		}
	}
	//si no encontre candidata, repito el paso dos (me quedaria con la primer pagina de la cola)
	if(!encontroCandidata){
		paginaAEliminar = queue_pop(tablaDePaginas);
	}
	return paginaAEliminar;
}

PaginaFrame * busquedaPaginaAEliminar(t_queue * tablaDePaginas){
	PaginaFrame * paginaAEliminar = malloc(sizeof(PaginaFrame));
	switch(configuracion->algoritmoDeReemplazo[0]){
		case 'F':
			paginaAEliminar = eliminarPorFifo(tablaDePaginas);
			break;
		case 'L':
			paginaAEliminar = eliminarPorLRS(tablaDePaginas);
			break;
		case 'C':
			paginaAEliminar = eliminarporClockM(tablaDePaginas);
			break;
	}
	return paginaAEliminar;
}

void busquedaPagina(t_queue * tablaDePaginas, char* numeroPagina, RespuestaBusquedaTablaPaginas* respuestaBusqueda, int seModifico){
	respuestaBusqueda->encontro = false;
	PaginaFrame* pagina = malloc(sizeof(PaginaFrame));
	int i = 0;
	switch(configuracion->algoritmoDeReemplazo[0]){
		case 'F':
			while(i < queue_size(tablaDePaginas)){
				pagina = queue_pop(tablaDePaginas);
				if(string_equals_ignore_case(pagina->pagina,numeroPagina)){
					respuestaBusqueda->encontro = true;
					if(seModifico) pagina->seModifico = 1;
					respuestaBusqueda->entradaBuscada = pagina;
				}
				queue_push(tablaDePaginas, pagina);
				i++;
			}
			break;
		case 'L':
			while(queue_size(tablaDePaginas) > i){
				pagina = queue_pop(tablaDePaginas);
				if(string_equals_ignore_case(pagina->pagina,numeroPagina)){
					respuestaBusqueda->encontro = true;
					if(seModifico) pagina->seModifico = 1;
					respuestaBusqueda->entradaBuscada = pagina;
					i--;
				} else{
					queue_push(tablaDePaginas, pagina);
				}
				i++;
			}
			if(respuestaBusqueda->encontro){
				queue_push(tablaDePaginas, respuestaBusqueda->entradaBuscada);
			}
			break;
		case 'C':
			while(queue_size(tablaDePaginas) > i){
				pagina = queue_pop(tablaDePaginas);
				if(string_equals_ignore_case(pagina->pagina,numeroPagina)){
					respuestaBusqueda->encontro = true;
					pagina->bitUso = 1;
					if(seModifico) pagina->seModifico = 1;
					respuestaBusqueda->entradaBuscada = pagina;
				}
				queue_push(tablaDePaginas, pagina);
				i++;
			}
			break;
	}
//	free(pagina);
}

void inicializar_diccionariosYSemaforos(){
	TLB = queue_create();

	tablaDeMarcos = list_create();
	int i = 0;
	while(i < configuracion->cantidad_marcos){
		Marco * marco = malloc(sizeof(Marco));
		marco->bitOcupado = 0;
		marco->contenidoMarco = "\0";
		list_add(tablaDeMarcos, marco);
		i++;
	}

	tablaProcesos = dictionary_create();
	estadisticasProcesos = dictionary_create();

	mutex_TLB = dar_mutex();
	mutex_tablaDeMarcos = dar_mutex();
	mutex_tablaDePaginas = dar_mutex();
	consultasTotales = 0;

	if(string_equals_ignore_case(configuracion->tlb_habilitada,"si")){
		consultasAcertadasPorTLB = 0;
	}
}

void setearNuevaEntrada(t_dictionary* diccionario, char* pid){
	t_queue* colaPaginas = queue_create();
	dictionary_put(diccionario, pid, colaPaginas);
	EstadisticasProceso * estadisticas = malloc(sizeof(EstadisticasProceso));
	estadisticas->cantidadPaginasAccedidas = 0;
	estadisticas->cantidadFallosDePagina = 0;
	estadisticas->accesosASwap = 0;
	dictionary_put(estadisticasProcesos, pid,(void*)estadisticas);
}

void buscarPaginaEnTLB(t_queue* TLB, char* pid, char* pagina, RespuestaBusquedaTLB * respuestaBusqueda){
	respuestaBusqueda->encontro = false;
	int i = 0;
	EntradaTLB * entradaTLB = malloc(sizeof(EntradaTLB));
	while(i < queue_size(TLB)){
		entradaTLB = queue_pop(TLB);
		if(string_equals_ignore_case(entradaTLB->proceso,pid) && string_equals_ignore_case(entradaTLB->pagina,pagina)){
			respuestaBusqueda->encontro = true;
			respuestaBusqueda->entradaBuscada = entradaTLB;
		}
		queue_push(TLB, entradaTLB);
		i++;
	}
}

void eliminarEntradaEnTLB(char* pid, char* pagina){
	int i = 0;
	EntradaTLB * entradaTLB = malloc(sizeof(EntradaTLB));
	while(i < queue_size(TLB)){
		entradaTLB = queue_pop(TLB);
		if(! (string_equals_ignore_case(entradaTLB->proceso,pid) && string_equals_ignore_case(entradaTLB->pagina,pagina))){
			queue_push(TLB, entradaTLB);
		}
		else{
			i--;
		}
		i++;
	}
}

void buscarPaginaEnTablaDeProceso(t_queue* tablaDePaginas, char* numeroPagina, RespuestaBusquedaTablaPaginas* respuestaBusqueda){
	respuestaBusqueda->encontro = false;
	int i = 0;
	PaginaFrame* pagina = malloc(sizeof(PaginaFrame));
	while(i < queue_size(tablaDePaginas)){
		pagina = queue_pop(tablaDePaginas);
		if(string_equals_ignore_case(pagina->pagina,numeroPagina)){
			respuestaBusqueda->encontro = true;
			respuestaBusqueda->entradaBuscada = pagina;
		}
		queue_push(tablaDePaginas, pagina);
		i++;
	}
	free(pagina);
}



void actualizarTLB(EntradaTLB * entradaTLB, char * pagina, char * pid, int marco){
	//comparo la cantidad de entradas de la tlb con el maximo permitido
	if(queue_size(TLB)< configuracion->entradas_tlb){
		queue_push(TLB, entradaTLB);
	} else {
		queue_pop(TLB);
		queue_push(TLB, entradaTLB);
	}
	int i = 0;
	char * infoTLBToLog = string_new();
	while(i < queue_size(TLB)){
		EntradaTLB * entrada = queue_pop(TLB);
		string_append(&infoTLBToLog, "\n----->Proceso:");
		string_append(&infoTLBToLog, entrada->proceso);
		string_append(&infoTLBToLog, ", Pagina:");
		string_append(&infoTLBToLog, entrada->pagina);
		string_append(&infoTLBToLog, ", Marco:");
		string_append(&infoTLBToLog, string_itoa(entrada->marco));
		string_append(&infoTLBToLog, ", Bit_modificado:");
		string_append(&infoTLBToLog, string_itoa(entrada->seModifico));
		queue_push(TLB, entrada);
		i++;
	}

	string_append(&infoTLBToLog, "\n");
	log_info(logMemoriaOculto, "Estado actual de la TLB: %s\n", infoTLBToLog);
	free(infoTLBToLog);
}

int buscarMarcoLibre(){
	int i = 0;
	Marco * marco = malloc(sizeof(Marco));
	while(i < configuracion->cantidad_marcos){
		marco = list_get(tablaDeMarcos, i);
		if(! marco->bitOcupado) return i;
		i++;
	}
	return -1;
}

bool agregarPaginaATablaDePaginas(t_queue * tablaDePaginas, char* numeroPaginaNueva, char* contenidoNuevo, char* pid, int seModifico, EstadisticasProceso * estadisticas){
	bool pudoAgregarPagina = false;
	PaginaFrame * paginaAAgregar = malloc(sizeof(PaginaFrame));
	paginaAAgregar->pagina = numeroPaginaNueva;
	paginaAAgregar->seModifico = seModifico;
	paginaAAgregar->bitUso = 1;
	pthread_mutex_lock(mutex_tablaDeMarcos);
	int marcoLibre = buscarMarcoLibre();
	//reviso si el proceso alcanzo su maximo de marcos posibles
	if((queue_size(tablaDePaginas) >= configuracion->maximo_marcos_por_proceso) ||
					(queue_size(tablaDePaginas) < configuracion->maximo_marcos_por_proceso && marcoLibre == -1 &&
					queue_size(tablaDePaginas) > 0)){
		//si lo alcanzo, aplico el algoritmo especificado en la configuracion
		pthread_mutex_unlock(mutex_tablaDeMarcos);
		PaginaFrame * paginaAEliminar = malloc(sizeof(PaginaFrame));
		paginaAEliminar = busquedaPaginaAEliminar(tablaDePaginas);
		paginaAAgregar->marco = paginaAEliminar->marco;
		Marco * marco = malloc(sizeof(Marco));
		pthread_mutex_lock(mutex_tablaDeMarcos);
		usleep(configuracion->retardo_memoria * 1000);
		marco = list_get(tablaDeMarcos, paginaAEliminar->marco);
		char* contenidoAnterior = marco->contenidoMarco;
		marco->contenidoMarco = contenidoNuevo;
		pthread_mutex_unlock(mutex_tablaDeMarcos);
		log_info(logMemoriaOculto, "Acceso a memoria\nmProc: %s\npagina: %s\nmarco: %d\n", pid, paginaAAgregar->pagina, paginaAEliminar->marco);
		//si la pagina a eliminmar fue modificada, le envio un msj al swap para que actualice su contenido
		if(paginaAEliminar->seModifico){
			char* msjAEnviar = string_new();
			string_append(&msjAEnviar, "2|");
			string_append(&msjAEnviar, pid);
			string_append(&msjAEnviar, "|");
			string_append(&msjAEnviar, paginaAEliminar->pagina);
			string_append(&msjAEnviar, "|");
			string_append(&msjAEnviar, contenidoAnterior);
			estadisticas->accesosASwap ++;
			enviar_string(socketClienteSwap->fd, msjAEnviar);
			char* respuesta = recibir_y_deserializar_mensaje(socketClienteSwap->fd);
		}
		queue_push(tablaDePaginas, paginaAAgregar);
		//actualizo TLB
		if(string_equals_ignore_case(configuracion->tlb_habilitada,"si")){
			pthread_mutex_lock(mutex_TLB);
			eliminarEntradaEnTLB(pid, paginaAEliminar->pagina);
			pthread_mutex_unlock(mutex_TLB);
			EntradaTLB * nuevaEntradaTLB = malloc(sizeof(EntradaTLB));
			nuevaEntradaTLB->marco = paginaAAgregar->marco;
			nuevaEntradaTLB->pagina = paginaAAgregar->pagina;
			nuevaEntradaTLB->proceso = pid;
			nuevaEntradaTLB->seModifico = seModifico;
			pthread_mutex_lock(mutex_TLB);
			actualizarTLB(nuevaEntradaTLB, paginaAAgregar->pagina, pid, paginaAAgregar->marco);
			pthread_mutex_unlock(mutex_TLB);
		}
		free(paginaAEliminar);
		pudoAgregarPagina = true;
	} else {
		Marco * marco = malloc(sizeof(Marco));
		if(marcoLibre >= 0){
			usleep(configuracion->retardo_memoria * 1000);
			marco = list_get(tablaDeMarcos, marcoLibre);
			marco->contenidoMarco = contenidoNuevo;
			marco->bitOcupado = 1;
		}
		pthread_mutex_unlock(mutex_tablaDeMarcos);
		if(marcoLibre >= 0){
			log_info(logMemoriaOculto, "acceso a memoria\nmproc: %s\npagina: %s\nmarco: %d\n", pid, numeroPaginaNueva, marcoLibre);
			paginaAAgregar->marco = marcoLibre;
			queue_push(tablaDePaginas, paginaAAgregar);
			if(string_equals_ignore_case(configuracion->tlb_habilitada,"si")){
				EntradaTLB * nuevaEntradaTLB = malloc(sizeof(EntradaTLB));
				nuevaEntradaTLB->marco = paginaAAgregar->marco;
				nuevaEntradaTLB->pagina = paginaAAgregar->pagina;
				nuevaEntradaTLB->proceso = pid;
				nuevaEntradaTLB->seModifico = seModifico;
				pthread_mutex_lock(mutex_TLB);
				actualizarTLB(nuevaEntradaTLB, paginaAAgregar->pagina, pid, paginaAAgregar->marco);
				pthread_mutex_unlock(mutex_TLB);
			}
			pudoAgregarPagina = true;
		}
	}
	return pudoAgregarPagina;
}

void mostrarEstadoTDP(t_queue * tablaDePaginas, char * estado, char * pidProceso){
	char * log = string_new();
	int i = 0;
	string_append(&log, estado);
	string_append(&log, " de la Tabla de Paginas del proceso ");
	string_append(&log, pidProceso);
	string_append(&log, " :\n");
	PaginaFrame * pagina = malloc(sizeof(PaginaFrame));
	while(i < queue_size(tablaDePaginas)){
		pagina = queue_pop(tablaDePaginas);
		string_append(&log, "----> Pagina = " );
		string_append(&log, pagina->pagina);
		string_append(&log, ", Marco = ");
		string_append(&log, string_itoa(pagina->marco));
		string_append(&log, ", bitModificado = ");
		string_append(&log, string_itoa(pagina->seModifico));
		if(string_equals_ignore_case(configuracion->algoritmoDeReemplazo, "CLOCK_M")) {
			string_append(&log, ", bitDeUso = ");
			string_append(&log, string_itoa(pagina->bitUso));
		}
		string_append(&log, "\n");
		queue_push(tablaDePaginas, pagina);
		i ++;
	}
	string_append(&log, "\n");
	log_info(logMemoriaOculto, log);
}

void finalizarProceso(char* pid){

	char* msjAEnviar = string_new();
	string_append(&msjAEnviar, "5|");
	string_append(&msjAEnviar, pid);
	enviar_string(socketClienteSwap->fd, msjAEnviar);
	char* respuesta = recibir_y_deserializar_mensaje(socketClienteSwap->fd);

	EstadisticasProceso * estadisticas = malloc(sizeof(EstadisticasProceso));
	estadisticas = dictionary_get(estadisticasProcesos, pid);
	log_info(logMemoriaOculto, "La cantidad de fallos de pagina del proceso %s fue de %d frente a %d pedido/s de pagina/s, con %d accesos a Swap\n",
			pid, estadisticas->cantidadFallosDePagina, estadisticas->cantidadPaginasAccedidas, estadisticas->accesosASwap);
	free(estadisticas);
	dictionary_remove(estadisticasProcesos, pid);

	 t_queue * tablaDePaginas = dictionary_get(tablaProcesos, pid);
	 while(0 < queue_size(tablaDePaginas)){
		 PaginaFrame * pagina = queue_pop(tablaDePaginas);
		 pthread_mutex_lock(mutex_tablaDeMarcos);
		 Marco * marco = malloc(sizeof(Marco));
		 marco = list_get(tablaDeMarcos, pagina->marco);
		 marco->bitOcupado = 0;
		 marco->contenidoMarco = "\0";
		 pthread_mutex_unlock(mutex_tablaDeMarcos);
		 free(pagina);
	 }

	queue_destroy(tablaDePaginas);

	dictionary_remove(tablaProcesos, pid);
	pthread_mutex_lock(mutex_TLB);
	pthread_mutex_unlock(mutex_TLB);
}

int32_t handshake(sock_t* socketToSend, int32_t codeRequest){
	int32_t code;
	int32_t messageSend = send(socketToSend->fd, &codeRequest, sizeof(int32_t),0);
	printf("handshake enviado: %d\n", codeRequest);
	if(messageSend!=-1){
		printf( "Se realizo correctamente el handshake \n");
		code=0;
	}
	else{
		printf( "No se realizo correctamente el handshake \n");
		code=-1;
	}
	return code;
}

Swap_struct* crearParametrosSwap(memoria* config, t_log* logSwap){
	Swap_struct* argSwap = malloc(sizeof(Swap_struct));
	argSwap->puerto = config->puerto_swap;
//	log_info(logSwap, "Puerto seteado para el Swap : %d\n", argSwap->puerto);
	argSwap->logger = logSwap;
	argSwap->ip = config->ip_swap;
//	log_info(logSwap, "IP seteado para el Swap : %s\n", argSwap->ip);
	return argSwap;
}

CPU_struct* crearParametrosCPU(memoria* config, t_log* logCPU){
	CPU_struct* argJob = malloc(sizeof(CPU_struct));
	argJob->puertoCPU = config->puerto_escucha;
	argJob->puertoSwap = config->puerto_swap;
	argJob->logger = logCPU;
	argJob->ip_Swap = config->ip_swap;
//	log_info(logCPU, "Puerto seteado para CPUListener : %d\n", argJob->puertoCPU);
	return argJob;
}

void crearHiloCPUListener(pthread_t* CPUThread, CPU_struct* argCPU, t_log* logMemoriaOculto){
	int32_t Jth;
	Jth = pthread_create(CPUThread, NULL, hiloCPUListener, (void*) argCPU);
	if (Jth != 0) {
		log_info(logMemoriaOculto, "Error al crear el hilo del CPUListener. Devolvio: %d\n", Jth);
	} else {
		log_info(logMemoriaOculto, "Se creo exitosamente el hilo del CPUListener\n");
	}
	EXIT_SUCCESS;
}


void * hiloPorcentajeDeAciertosDeTLB(){
	while(1){
		usleep(60000000);
		if(consultasTotales != 0){
			printf("Porcentaje de aciertos historicos de la TLB: %f %\n", ((double)consultasAcertadasPorTLB/consultasTotales)*100);
		} else {
			printf("No se han hecho consultas a la TLB aun\n");
		}
	}
}

void crearHiloEstadisticasTLB(pthread_t* estadisticasTLBThread, CPU_struct* argCPU, t_log* logMemoriaOculto){
	int32_t Jth;
	Jth = pthread_create(estadisticasTLBThread, NULL, hiloPorcentajeDeAciertosDeTLB, (void*) NULL);
	if (Jth != 0) {
		log_info(logMemoriaOculto, "Error al crear el hilo de estadisticas. Devolvio: %d\n", Jth);
	} else {
		log_info(logMemoriaOculto, "Se creo exitosamente el hilo de porcentajes de la TLB\n");
	}
	EXIT_SUCCESS;
}

void  SwapFunction(void * parametros){
	Swap_struct* argSwap = malloc(sizeof(Swap_struct));
	argSwap =  (Swap_struct*) parametros;
//	log_info(argSwap->logger,"Inicio hilo Swap\n");

	//printf("Puerto swap %d\n", argSwap->puerto);
	socketClienteSwap = create_client_socket(argSwap->ip, argSwap->puerto);
	//printf("Socket cliente: %d \n", socketClienteSwap->fd);
	int32_t conectadoASwap = connect_to_server(socketClienteSwap);
	if(conectadoASwap < 0){
		//log_error(loggerError, "Error al conectar el socket del proceso Memoria\nSe bajara el proceso\n");
		log_info(logMemoriaOculto,"No se ha podido conectar con el proceso swap\n");
		seCreoElClienteSwap = false;

		EXIT_FAILURE;
	}else{
		log_info(argSwap->logger,"Se creo la conexion con el Swap\n");
		//enviar_string(socketClienteSwap->fd, "1|10|5");
		seCreoElClienteSwap = true;
		EXIT_SUCCESS;
	}
}


void * hiloCPUListener(void* parametros){

	CPU_struct* argCPU = malloc(sizeof(CPU_struct));
	argCPU = (CPU_struct*) parametros;
	log_info(argCPU->logger, "Inicio hilo CPU listener\n");

	//set of socket descriptors
	fd_set cpus;

	//Somos servidores
	sock_t* socketServerCPU = malloc(sizeof(sock_t));
	socketServerCPU = create_server_socket(argCPU->puertoCPU);
	fd_set readfds;
	listen_connections(socketServerCPU);
	int32_t accept = 1;
	//clear the socket set
	FD_ZERO(&readfds);
	FD_ZERO(&cpus);

	//add master socket to set
	FD_SET(socketServerCPU->fd, &cpus);
	int32_t socket_cpu_new = socketServerCPU->fd;
	int32_t fdmax = socket_cpu_new;
	int i;
	pthread_t hiloNCPU;
	pthread_t thread_procesador_de_mensaje;

	finalizarMemoria = false;
	while(! finalizarMemoria){
		// Copia el conjunto maestro al temporal

		readfds = cpus;

		if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) {
			log_info(logMemoriaOculto, "Error en el select \n");
			exit(1);
		}

		for(i = 1; i <= fdmax; i++) {
			 // Si hay datos entrantes en el socket i
			if (FD_ISSET(i, &readfds)) {
				//Acepto la conexion entrante
				sock_t * socket_cpu = accept_connection(socketServerCPU);
				FD_SET(socket_cpu_new, &cpus);

				// Actualiza el socket maximo
				if (socket_cpu->fd > fdmax)
					fdmax = socket_cpu->fd;
				char * mensajeCrudo = recibir_y_deserializar_mensaje(socket_cpu->fd);
				char codigoProceso = mensajeCrudo[0];
				char** mensajePartido = string_split(mensajeCrudo, "|");
				bool encontro = false;
				MsjRecibido * msjSeteado = malloc(sizeof(MsjRecibido));
				t_queue * tablaDePaginas;
				char *contenido = malloc(sizeof(char*));
				EstadisticasProceso * estadisticas;

				//Identifico mensaje recibido y procedo
				switch(codigoProceso){
					//setea las estructuras para el nuevo mProc
					case '1':
						msjSeteado->pid = mensajePartido[1];
						msjSeteado->pagina = mensajePartido[2];
						//envio el msj de creacion a swap
						enviar_string(socketClienteSwap->fd, mensajeCrudo);
						char* respuesta = recibir_y_deserializar_mensaje(socketClienteSwap->fd);
						if(string_equals_ignore_case(respuesta, "0")){
							setearNuevaEntrada(tablaProcesos, mensajePartido[1]);
						}else{
							log_info(logMemoriaOculto, "No pudo iniciarse el mProc con PID %s\n", mensajePartido[1]);
							enviar_string(socket_cpu->fd, "0");
							break;
						}

						log_info(logMemoriaOculto, "Se creo un nuevo mProc con PID %s y con %s pagina/s\n", msjSeteado->pid, msjSeteado->pagina);
						enviar_string(socket_cpu->fd, "1");
						break;
					//pedido de lectura
					case '3':
						estadisticas = dictionary_get(estadisticasProcesos, mensajePartido[1]);
						consultasTotales ++;
						msjSeteado->pid = mensajePartido[1];
						msjSeteado->pagina = mensajePartido[2];
						log_info(logMemoriaOculto, "Pedido de lectura del procceso %s de la pagina %s\n", msjSeteado->pid, msjSeteado->pagina);
						estadisticas->cantidadPaginasAccedidas ++;
						char * lectura = string_new();
						string_append(&lectura, "1|");
						//busco en TLB
						if(string_equals_ignore_case(configuracion->tlb_habilitada,"si")){
							RespuestaBusquedaTLB * respuestaBusqueda = malloc(sizeof(RespuestaBusquedaTLB));
							pthread_mutex_lock(mutex_TLB);
							buscarPaginaEnTLB(TLB, msjSeteado->pid, msjSeteado->pagina, respuestaBusqueda);
							pthread_mutex_unlock(mutex_TLB);
							//si encuentro la pagina del proceso, busco el contenido del marco
							if(respuestaBusqueda->encontro){
								consultasAcertadasPorTLB ++;
								log_info(logMemoriaOculto, "TLB hit. El marco resultante fue %d", respuestaBusqueda->entradaBuscada->marco);
								encontro = true;
								log_info(logMemoriaOculto, "Acceso a memoria\nmProc: %s\npagina: %s\nmarco: %d\n", msjSeteado->pid, msjSeteado->pagina, respuestaBusqueda->entradaBuscada->marco);
								pthread_mutex_lock(mutex_tablaDeMarcos);
								usleep(configuracion->retardo_memoria * 1000);
								Marco * marco = malloc(sizeof(Marco));
								marco = list_get(tablaDeMarcos, respuestaBusqueda->entradaBuscada->marco);
								contenido = marco->contenidoMarco;
								pthread_mutex_unlock(mutex_tablaDeMarcos);
								string_append(&lectura, contenido);
								if(! string_equals_ignore_case(configuracion->algoritmoDeReemplazo, "fifo")){
									RespuestaBusquedaTablaPaginas * respuestaBusquedaMem = malloc(sizeof(RespuestaBusquedaTablaPaginas));
									tablaDePaginas = dictionary_get(tablaProcesos, msjSeteado->pid);
									pthread_mutex_lock(mutex_tablaDePaginas);
									//TODO aca hay reordenamiento de colas... deberia hacer un retardo de memoria???
									mostrarEstadoTDP(tablaDePaginas, "Estado inicial", msjSeteado->pid);
									busquedaPagina(tablaDePaginas, msjSeteado->pagina, respuestaBusquedaMem, 0);
									mostrarEstadoTDP(tablaDePaginas, "Estado Final", msjSeteado->pid);
									pthread_mutex_unlock(mutex_tablaDePaginas);
									respuestaBusquedaMem->entradaBuscada->bitUso = 1;
								}
//								log_info(logMemoriaOculto, "Pedido de lectura de la pagina %s del mProc con PID %s. TLB hit. Se accedio al marco %d\n",
//										msjSeteado->pagina, msjSeteado->pid, respuestaBusqueda->entradaBuscada->marco);
							}
							else{
								usleep(configuracion->retardo_memoria * 1000);
							}
						}
						//si todavia no encontro la pagina buscada, la busco en memoria
						if(! encontro){
							if(string_equals_ignore_case(configuracion->tlb_habilitada, "si")){
								log_info(logMemoriaOculto, "TLB miss. Comienza busqueda de la pagina en Tabla de Paginas");
							} else {
								log_info(logMemoriaOculto, "Comienza busqueda de la pagina en Tabla de Paginas");
							}
							RespuestaBusquedaTablaPaginas * respuestaBusqueda = malloc(sizeof(RespuestaBusquedaTablaPaginas));
							tablaDePaginas = dictionary_get(tablaProcesos, msjSeteado->pid);
							pthread_mutex_lock(mutex_tablaDePaginas);
							usleep(configuracion->retardo_memoria * 1000);
							busquedaPagina(tablaDePaginas, msjSeteado->pagina, respuestaBusqueda, 0);
							pthread_mutex_unlock(mutex_tablaDePaginas);
							//si encuentro la pagina del proceso, busco el contenido del marco
							if(respuestaBusqueda->encontro){
								encontro = true;
								log_info(logMemoriaOculto, "Se logro encontrar la pagina en la Tabla de Paginas del proceso");
								respuestaBusqueda->entradaBuscada->bitUso = 1;
								log_info(logMemoriaOculto, "Acceso a memoria\nmProc: %s\npagina: %s\nmarco: %d\n", msjSeteado->pid, msjSeteado->pagina, respuestaBusqueda->entradaBuscada->marco);
								pthread_mutex_lock(mutex_tablaDeMarcos);
								Marco * marco = malloc(sizeof(Marco));
								usleep(configuracion->retardo_memoria * 1000);
								marco = list_get(tablaDeMarcos, respuestaBusqueda->entradaBuscada->marco);
								contenido = marco->contenidoMarco;
								pthread_mutex_unlock(mutex_tablaDeMarcos);
								//actualizo TLB
								if(string_equals_ignore_case(configuracion->tlb_habilitada,"si")){
									EntradaTLB * nuevaEntradaTLB = malloc(sizeof(EntradaTLB));
									nuevaEntradaTLB->marco = respuestaBusqueda->entradaBuscada->marco;
									nuevaEntradaTLB->pagina = msjSeteado->pagina;
									nuevaEntradaTLB->proceso = msjSeteado->pid;
									nuevaEntradaTLB->seModifico = respuestaBusqueda->entradaBuscada->seModifico;
									pthread_mutex_lock(mutex_TLB);
									actualizarTLB(nuevaEntradaTLB, msjSeteado->pagina, msjSeteado->pid, respuestaBusqueda->entradaBuscada->marco);
									pthread_mutex_unlock(mutex_TLB);
								} else{
//									log_info(logMemoriaOculto, "Pedido de lectura de la pagina %s del mProc con PID %s. TLB deshabilitada. Se accedio al marco %d\n",
//										msjSeteado->pagina, msjSeteado->pid, respuestaBusqueda->entradaBuscada->marco);
								}
								string_append(&lectura, contenido);
							}
						}
						//si no encontro la pagina buscada, se la pido a Swap
						if(! encontro){
							log_info(logMemoriaOculto, "Fallo de Pagina. Enviando solicitud de la pagina %s del mProc %s al Swap", msjSeteado->pagina, msjSeteado->pid);
							estadisticas->cantidadFallosDePagina ++;
							estadisticas->accesosASwap ++;
							enviar_string(socketClienteSwap->fd, mensajeCrudo);
							char* respuesta = recibir_y_deserializar_mensaje(socketClienteSwap->fd);
							MsjRecibido * respuestaSerializada = malloc(sizeof(MsjRecibido));
							char** respuestaSeparada = string_split(respuesta, "|");
							respuestaSerializada->codigoDelProceso = respuestaSeparada[0];
							tablaDePaginas = dictionary_get(tablaProcesos, msjSeteado->pid);
							//si pudo devolverme la pagina, la agrego (en teoria no hay pedidos de paginas invalidos)
							//TODO posible mejora: terminar el proceso ante un pedido de lectura de pagina invalido
							pthread_mutex_lock(mutex_tablaDePaginas);
							usleep(configuracion->retardo_memoria * 1000);
							mostrarEstadoTDP(tablaDePaginas, "Estado inicial", msjSeteado->pid);
							bool pudoAgregarPagina = agregarPaginaATablaDePaginas(tablaDePaginas, msjSeteado->pagina, respuestaSeparada[0], msjSeteado->pid, 0, estadisticas);
							if(pudoAgregarPagina) mostrarEstadoTDP(tablaDePaginas, "Estado final", msjSeteado->pid);
							pthread_mutex_unlock(mutex_tablaDePaginas);
							//si no pudo agregarla, termina el proceso
							if(! pudoAgregarPagina){
								log_info(logMemoriaOculto, "Finaliza el mProc %s debido a que no quedan marcos disponibles\n", msjSeteado->pid);
								enviar_string(socket_cpu->fd, "0");
								finalizarProceso(msjSeteado->pid);
								break;
							}
							string_append(&lectura, respuestaSeparada[0]);
//							//si pudo devolverme la pagina, la agrego, si no, el proceso deberia terminar (creo...)
//							if(respuestaSerializada->codigoDelProceso){
//								respuestaSerializada->stringAEscribir = respuestaSeparada[1];
//								pthread_mutex_lock(mutex_tablaDePaginas);
//								bool pudoAgregarPagina = agregarPaginaATablaDePaginas(tablaDePaginas, msjSeteado->pagina, respuestaSerializada->stringAEscribir, msjSeteado->pid, 0);
//								pthread_mutex_unlock(mutex_tablaDePaginas);
//								if(! pudoAgregarPagina){
//									log_info(logMemoriaOculto, "Finaliza el mProc debido a que no quedan marcos disponibles");
//									finalizarProceso(msjSeteado->pid);
//									break;
//								}
//								enviar_string(socket_cpu->fd, respuestaSerializada->stringAEscribir);
//							} else {
//								//TODO habria que terminar el proceso??
//							}
						}
						enviar_string(socket_cpu->fd, lectura);
						log_info(logMemoriaOculto, "Finaliza pedido de lectrura del proceso %s, de la pagina %s\n", msjSeteado->pid, msjSeteado->pagina);
						break;
					//pedido de escritura
					case '2':
						estadisticas = dictionary_get(estadisticasProcesos, mensajePartido[1]);
						consultasTotales ++;
						msjSeteado->pid = mensajePartido[1];
						msjSeteado->pagina = mensajePartido[2];
						msjSeteado->stringAEscribir = mensajePartido[3];
						estadisticas->cantidadPaginasAccedidas = estadisticas->cantidadPaginasAccedidas + 1;
						log_info(logMemoriaOculto, "Pedido de escritura del procceso %s de la pagina %s. String a escribir: %s\n", msjSeteado->pid, msjSeteado->pagina, msjSeteado->stringAEscribir);
						//busco en TLB
						if(string_equals_ignore_case(configuracion->tlb_habilitada,"SI")){
							RespuestaBusquedaTLB * respuestaBusqueda = malloc(sizeof(RespuestaBusquedaTLB));
							pthread_mutex_lock(mutex_TLB);
							buscarPaginaEnTLB(TLB, msjSeteado->pid, msjSeteado->pagina, respuestaBusqueda);
							pthread_mutex_unlock(mutex_TLB);
							//si encuentro la pagina del proceso, escribo el nuevo contenido en el marco y actualizo bit de modificado de tabla de paginas de ser necesario
							if(respuestaBusqueda->encontro){
								encontro = true;
								log_info(logMemoriaOculto, "TLB hit. El marco resultante fue %d", respuestaBusqueda->entradaBuscada->marco);
								log_info(logMemoriaOculto, "Acceso a memoria\nmProc: %s\npagina: %s\nmarco: %d\n", msjSeteado->pid, msjSeteado->pagina, respuestaBusqueda->entradaBuscada->marco);
								pthread_mutex_lock(mutex_tablaDeMarcos);
								usleep(configuracion->retardo_memoria * 1000);
								Marco * marco = malloc(sizeof(Marco));
								marco = list_get(tablaDeMarcos, respuestaBusqueda->entradaBuscada->marco);
								marco->contenidoMarco = msjSeteado->stringAEscribir;
								pthread_mutex_unlock(mutex_tablaDeMarcos);
								if(! respuestaBusqueda->entradaBuscada->seModifico){
									RespuestaBusquedaTablaPaginas * respuestaBusquedaMem = malloc(sizeof(RespuestaBusquedaTablaPaginas));
									tablaDePaginas = dictionary_get(tablaProcesos, msjSeteado->pid);
									pthread_mutex_lock(mutex_tablaDePaginas);
									mostrarEstadoTDP(tablaDePaginas, "Estado inicial", msjSeteado->pid);
									busquedaPagina(tablaDePaginas, msjSeteado->pagina, respuestaBusquedaMem, 0);
									mostrarEstadoTDP(tablaDePaginas, "Estado final", msjSeteado->pid);
									pthread_mutex_unlock(mutex_tablaDePaginas);
									respuestaBusquedaMem->entradaBuscada->bitUso=1;
									respuestaBusquedaMem->entradaBuscada->seModifico = 1;
									respuestaBusqueda->entradaBuscada->seModifico = 1;
								}
							}
							else{
								usleep(configuracion->retardo_memoria * 1000);
							}
						}
						//si todavia no encontro la pagina buscada, la busco en memoria
						if(! encontro){
							if(string_equals_ignore_case(configuracion->tlb_habilitada, "si")){
								log_info(logMemoriaOculto, "TLB miss. Comienza busqueda de la pagina en Tabla de Paginas");
							} else {
								log_info(logMemoriaOculto, "Comienza busqueda de la pagina en Tabla de Paginas");
							}
							RespuestaBusquedaTablaPaginas * respuestaBusqueda = malloc(sizeof(RespuestaBusquedaTablaPaginas));
							t_queue* tablaDePaginas = dictionary_get(tablaProcesos, msjSeteado->pid);
							pthread_mutex_lock(mutex_tablaDePaginas);
							usleep(configuracion->retardo_memoria * 1000);
							busquedaPagina(tablaDePaginas, msjSeteado->pagina, respuestaBusqueda, 1);
							pthread_mutex_unlock(mutex_tablaDePaginas);
							//si encuentro la pagina del proceso, busco el contenido del marco
							if(respuestaBusqueda->encontro){
								encontro = true;
								log_info(logMemoriaOculto, "Se logro encontrar la pagina en la Tabla de Paginas del proceso");
								pthread_mutex_lock(mutex_tablaDeMarcos);
								usleep(configuracion->retardo_memoria * 1000);
								Marco * marco = malloc(sizeof(Marco));
								marco = list_get(tablaDeMarcos, respuestaBusqueda->entradaBuscada->marco);
								marco->contenidoMarco = msjSeteado->stringAEscribir;
								pthread_mutex_unlock(mutex_tablaDeMarcos);
								log_info(logMemoriaOculto, "Acceso a memoria\nmProc: %s\npagina: %s\nmarco: %d\n", msjSeteado->pid, msjSeteado->pagina, respuestaBusqueda->entradaBuscada->marco);
								respuestaBusqueda->entradaBuscada->bitUso = 1;
								respuestaBusqueda->entradaBuscada->seModifico = 1;
								//actualizo TLB
								if(string_equals_ignore_case(configuracion->tlb_habilitada,"si")){
									EntradaTLB * nuevaEntradaTLB = malloc(sizeof(EntradaTLB));
									nuevaEntradaTLB->marco = respuestaBusqueda->entradaBuscada->marco;
									nuevaEntradaTLB->pagina = msjSeteado->pagina;
									nuevaEntradaTLB->proceso = msjSeteado->pid;
									nuevaEntradaTLB->seModifico = 1;
									pthread_mutex_lock(mutex_TLB);
									actualizarTLB(nuevaEntradaTLB, msjSeteado->pagina, msjSeteado->pid, respuestaBusqueda->entradaBuscada->marco);
									pthread_mutex_unlock(mutex_TLB);
								}
							}
						}
						//si no encontro la pagina buscada, se la pido a Swap
						if(! encontro){
							log_info(logMemoriaOculto, "Fallo de Pagina. Enviando solicitud de la pagina %s del mProc %s al Swap", msjSeteado->pagina, msjSeteado->pid);
							estadisticas->cantidadFallosDePagina ++;
							char* pedidoASwap = string_new();
							string_append(&pedidoASwap, "3|");
							string_append(&pedidoASwap,msjSeteado->pid );
							string_append(&pedidoASwap, "|");
							string_append(&pedidoASwap,msjSeteado->pagina  );
							estadisticas->accesosASwap ++;
							enviar_string(socketClienteSwap->fd, pedidoASwap);
							char* respuesta = recibir_y_deserializar_mensaje(socketClienteSwap->fd);
							tablaDePaginas = dictionary_get(tablaProcesos, msjSeteado->pid);
							//si pudo devolverme la pagina, la agrego (en teoria no hay pedidos de paginas invalidos)
							//TODO posible mejora: terminar el proceso ante un pedido de lectura de pagina invalido
							pthread_mutex_lock(mutex_tablaDePaginas);
							usleep(configuracion->retardo_memoria * 1000);
							mostrarEstadoTDP(tablaDePaginas, "Estado inicial", msjSeteado->pid);
							bool pudoAgregarPagina = agregarPaginaATablaDePaginas(tablaDePaginas, msjSeteado->pagina, msjSeteado->stringAEscribir, msjSeteado->pid, 1, estadisticas);
							if(pudoAgregarPagina) mostrarEstadoTDP(tablaDePaginas, "Estado final", msjSeteado->pid);
							pthread_mutex_unlock(mutex_tablaDePaginas);
							//si no pudo agregarla, termina el proceso
							if(! pudoAgregarPagina){
								log_info(logMemoriaOculto, "Finaliza el mProc %s debido a que no quedan marcos disponibles", msjSeteado->pid);
								enviar_string(socket_cpu->fd, "0");
								finalizarProceso(msjSeteado->pid);
								break;
							}
						}
						enviar_string(socket_cpu->fd, "1");
						log_info(logMemoriaOculto, "finaliza pedido de escritura del proceso %s de la pagina %s\n", msjSeteado->pid, msjSeteado->pagina);
						break;
					case '5':
						msjSeteado->pid = mensajePartido[1];
						log_info(logMemoriaOculto, "Finaliza el mProc %s", msjSeteado->pid);
						finalizarProceso(msjSeteado->pid);
						break;
					case '6':
						log_info(logMemoriaOculto, "Finalizan los hilos CPU, cerrando Proceso Memoria\n");
						enviar_string(socketClienteSwap->fd, "6");
						cerrarMemoria();
						exit(EXIT_SUCCESS);
						break;
					case '7':
						log_info(logMemoriaOculto, "Se ha conectado un nuevo hilo CPU\n");
						break;
					default:
						break;
				}
			}
		}
	}

}
