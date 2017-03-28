#include "funcionesPlanificador.h"
#include <commons/collections/dictionary.h>

pthread_mutex_t mutex_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_pcb = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_exit = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_io = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_uso_cola_cpu = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_cpus = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_dic_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_cpu_ocupadas = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_ejecucion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_espera = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_respuesta = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_mensajes = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_general = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_get_pcb = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_quantum = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_socket = PTHREAD_MUTEX_INITIALIZER;

planificador * levantarConfigPlanificador(char * path) {
	t_config * archConfig = config_create(path);
	planificador * config = malloc(sizeof(planificador));
	config->puerto_escucha = config_get_int_value(archConfig, "PUERTO_ESCUCHA");
	config->quantum = config_get_int_value(archConfig, "QUANTUM");
	config->algortimo_planificacion = strdup(config_get_string_value(archConfig, "ALGORITMO_PLANIFICACION"));
	config_destroy(archConfig);
	return config;
}

void inicializar_diccionarios_y_colas() {
	cola_cpu_disponibles = list_create();
	cola_cpu_conectadas = list_create();
	cpus_c = 0;
	crear_cont(&cantidad_listos, 0);
	crear_cont(&cantidad_block, 0);
	crear_cont(&cantidad_exec, 0);
	crear_cont(&cantidad_finish, 0);
	crear_cont(&cantidad_cpu_libres, 0);
	crear_cont(&cantidad_io, 0);
	crear_cont(&archivo_ok, 0);
	crear_cont(&io_ok, 0);
	crear_cont(&listo_ok, 1);
	crear_cont(&ejec_ok, 0);

	quit_sistema = 1;
	pids = 1;

}

CPU_struct * crearParametrosCPU(planificador * config, t_log * logCPU) {
	CPU_struct* argCPU = malloc(sizeof(CPU_struct));
	argCPU->puertoCPU = config->puerto_escucha;
	argCPU->logger = logCPU;
	return argCPU;
}

int32_t crearHiloCPUListener(pthread_t* CPUThread, CPU_struct * argCPU,
		t_log * logPlanificador) {
	int32_t Jth;
	Jth = pthread_create(CPUThread, NULL, hiloCPUListener, (void*) argCPU);
	if (Jth != 0)
		log_info(logPlanificador,"Error al crear el hilo del CPUListener. Devolvio: %d\n", Jth);
	else
		log_info(logPlanificador,"Se creo exitosamente el hilo del CPUListener");

	return Jth;
}

void crear_colas_procesos(){

	//colas
	dic_cpus = dictionary_create();
	tiempos_ejecucion = dictionary_create();
	tiempos_espera = dictionary_create();
	tiempos_respuesta = dictionary_create();
	cola_procesos_ejecutandose = queue_create();
	cola_procesos_finalizados = queue_create();
	cola_procesos_listos = queue_create();
	cola_procesos_suspendidos = queue_create();
	cola_procesos_io = queue_create();
	cola_cpu = queue_create();
	cola_cpu_ocupadas = queue_create();
}

void * hiloCPUListener(void* parametros) {
	CPU_struct* argCPU = malloc(sizeof(CPU_struct));
	argCPU = (CPU_struct*) parametros;

	//Creo hilo manejador de E/S
	pthread_t  hilo_io;
	pthread_create(&hilo_io, NULL,(void *) manejador_IO, NULL);

	//Creo hilo manejador de ready_yo_exec
	pthread_t  hilo_ready_exec;
	pthread_create(&hilo_ready_exec, NULL,(void *) manejador_fin_quantum, NULL);
	//set of socket descriptors
	fd_set readfds;
	fd_set cpus;

	crear_colas_procesos();

	//Somos servidores
	socketServerCPU = create_server_socket(argCPU->puertoCPU);
	listen_connections(socketServerCPU);
	//clear the socket set
	FD_ZERO(&readfds);
	FD_ZERO(&cpus);

	//add master socket to set
	FD_SET(socketServerCPU->fd, &cpus);
	int32_t socket_cpu_new = socketServerCPU->fd;
	int32_t fdmax = socket_cpu_new;
	int i;

	while (quit_sistema) {
		// Copia el conjunto maestro al temporal
		readfds = cpus;

		if (select(fdmax + 1, &readfds, NULL, NULL, NULL) == -1) {
			log_info(log_planificador, "Error en el select \n");
			exit(1);
		}

		for (i = 1; i <= fdmax; i++) {
			// Si hay datos entrantes en el socket i
			if (FD_ISSET(i, &readfds)) {
				//Acepto la conexion entrante
				sock_t * socket_cpu = accept_connection(socketServerCPU);
				FD_SET(socket_cpu_new, &cpus);

				// Actualiza el socket maximo
				if (socket_cpu->fd > fdmax)
					fdmax = socket_cpu->fd;

				//Recibo mje de alguna cpu
				char * mensaje_serializado = recibir_y_deserializar_mensaje(
						socket_cpu->fd);
				//puts(mensaje_serializado);
				char mje = mensaje_serializado[0];
				cpu_t * cpu;
				cpu_t * cpu_final, * cpu_quantum;
				char ** datos;

				//Identifico mensaje recibio y procedo
				switch (mje) {
				case 'N':
					//Vino una nueva cpu, la deserializo y la agrego a la cola de cpu disponibles y conectadas
					cpu = deserializar_cpu(mensaje_serializado);
					nueva_cpu(cpu, socket_cpu->fd);
					log_info(log_pantalla, "Se conecto CPU con ID: %s con Socket : %d",
							cpu->idCPU, cpu->fdCPU);
					break;
				case 'Q':
					//Q&ID_CPU&PCBSERIALIZADA&ProgramCounter&SentenciasEjecutadas se pasa a cola de ready por finalizacion de quantum
					datos = string_split(mensaje_serializado, "&");
					pthread_mutex_lock(&mutex_cpus);
						cpu_quantum = dictionary_get(dic_cpus, datos[1]);
					pthread_mutex_unlock(&mutex_cpus);
					pcb_t * pcb = deserializar_pcb(datos[2]);
					mostrar_mensaje_rafaga(datos[4], pcb->pid);
					fin_quantum(cpu_quantum, pcb, atoi(datos[3]));
					log_info(log_planificador,
							"[CPU ID: %s] mProc PID: %d terminó ejecución por quantum",
							cpu_quantum->idCPU, pcb->pid);
					break;
				case 'X':
					//Dieron de baja el proceso cpu, hay que sacarlar de la cola de cpus y a sus procesos tmb
					datos = string_split(mensaje_serializado, "|");
					pthread_mutex_lock(&mutex_cpus);
						dictionary_clean(dic_cpus);
						list_clean(cola_cpu_conectadas);
					pthread_mutex_unlock(&mutex_cpus);
					pthread_mutex_lock(&mutex_cpus);
						 dar_de_baja_cpu();
					pthread_mutex_unlock(&mutex_cpus);
					log_info(log_planificador,"Se cayeron los hilos CPU.");
					break;
				case 'E':
					//Recibo un mje de que una cpu entro a E/S:
					//E&IDCpu&Retardo&PcbSerializada&ProgramCounter&SentenciasEjecutadas
					datos = string_split(mensaje_serializado, "&");
					pthread_mutex_lock(&mutex_cpus);
						cpu_final = dictionary_get(dic_cpus, datos[1]);
					pthread_mutex_unlock(&mutex_cpus);
					pcb_t * pcb_io = deserializar_pcb(datos[3]);
					pcb_t * h = buscar_pcb_sin_sacarla(pcb_io->pid);
					mostrar_mensaje_rafaga(datos[5], h->pid);
					if((h->program_counter != (pcb_io->cant_var_cont_actual-1))&&
							!string_equals_ignore_case(h->estado, "F"))
						actualizar_pcb(pcb_io->pid, atoi(datos[4]),1);
					pasar_ejec_to_io(pcb_io->pid,atoi(datos[2]));
					break;
				case 'I':
					//I|Pcb|codigo|Total
					datos = string_split(mensaje_serializado, "&");
					char ** pcb_array = string_split(datos[1], "|");
					pthread_mutex_lock(&mutex_ejecucion);
					metrica * metrica = malloc(sizeof(metrica));
					metrica->inicial = time(NULL);
					metrica->total = 0;
					dictionary_put(tiempos_ejecucion, pcb_array[2], (void*)metrica);
					pthread_mutex_unlock(&mutex_ejecucion);
					if(atoi(datos[2]) == 0 || atoi(datos[2]) == 2){
						pthread_mutex_lock(&mutex_cpus);
						cpu_t * cpu_iniciar = buscar_cpu(atoi(pcb_array[2]));
						cpu_iniciar->id_proceso_exec = 0;
						queue_push(cola_cpu, (void *) cpu_iniciar);
						sem_post(&cantidad_cpu_libres);
						pthread_mutex_unlock(&mutex_cpus);
						run_to_finish(datos[1],cpu_iniciar->idCPU, 2, datos[1]);
					}
					if(atoi(datos[2]) == 1){
						actualizar_total_pcb(pcb_array[2], atoi(datos[3]));
						log_info(log_planificador,"El mProc con PID :%d inicio correctamente y posee :%d  sentencias para ejecutar",
								atoi(pcb_array[2]), atoi(datos[3]));
						sem_post(&archivo_ok);
					}
					free(pcb_array);
					break;
				case '5':
					//FInaliza la ejecucion de un proceso y recibo: F&IDCpu&PcbSerializada&SentenciasEjecutadas
					datos = string_split(mensaje_serializado, "&");
					pthread_mutex_lock(&mutex_cpus);
						cpu_final = dictionary_get(dic_cpus, datos[1]);
					pthread_mutex_unlock(&mutex_cpus);
					pthread_mutex_lock(&mutex_cpus);
						cpu_final->id_proceso_exec = 0;
						queue_push(cola_cpu, (void *) cpu_final);
						buscar_cpu_por_id(cpu_final->idCPU);
						sem_post(&cantidad_cpu_libres);
					pthread_mutex_unlock(&mutex_cpus);
					run_to_finish(datos[2],cpu_final->idCPU, 1, datos[3]);
					break;
				case 'P':
					//Recibo P|IDCpu|NuevoPorcentaje y procedo a actualizar las cpu con sus nuevo porcentaje de uso
					datos = string_split(mensaje_serializado, "|");
					pthread_mutex_lock(&mutex_cpus);
						cpu_final = dictionary_get(dic_cpus, datos[1]);
					pthread_mutex_unlock(&mutex_cpus);
					guardar_porcentaje_cpu(cpu_final, atoi(datos[2]));
					log_info(log_planificador,"Se actualizo el porcentaje de uso de la CPU ID: %s a : %d ", cpu_final->idCPU, atoi(datos[2]));
					cpus_c++;
					if(cpus_c == (queue_size(cola_cpu)+queue_size(cola_cpu_ocupadas)))
						mostrar_porcentaje();
					break;
				case 'A':
					datos = string_split(mensaje_serializado, "&");
					log_info(log_planificador,
								"[CPU ID: %s] Abrio el archivo correctamente:",datos[1]);
;					break;
				default:
					log_error(log_planificador,"Error con la cpu");
					break;
				}
			}
		}
	}
	return 0;
}

void nueva_cpu(cpu_t * cpu, int32_t nuevo_fd){
	if (cpu->fdCPU == -1) //Cuando es nueva
		cpu->fdCPU = nuevo_fd;
	pthread_mutex_lock(&mutex_cpus);
	list_add(cola_cpu_disponibles, cpu);
	list_add(cola_cpu_conectadas, cpu);
	sem_post(&cantidad_cpu_libres);
	queue_push(cola_cpu,(void *) cpu);
	dictionary_put(dic_cpus, cpu->idCPU, cpu);
	pthread_mutex_unlock(&mutex_cpus);
}

void actualizar_cpu(cpu_t * cpu){
	int32_t i = 0;
	pthread_mutex_lock(&mutex_cpu_ocupadas);
	int32_t longitud = queue_size(cola_cpu_ocupadas);
	for (i=0; i< longitud; i++){
		cpu_t * cpu_aux;
		cpu_aux = queue_pop(cola_cpu_ocupadas);
		if (cpu_aux->idCPU == cpu->idCPU){
			cpu_aux->fdCPU = cpu->fdCPU;
			queue_push(cola_cpu_ocupadas, (void*)cpu_aux);
			return;
		}else
			queue_push(cola_cpu_ocupadas,(void*)cpu_aux);
	}
	pthread_mutex_unlock(&mutex_cpu_ocupadas);
}

void agregar_pcb_cola_listos(pcb_t * pcb) {
	//Agrego proceso mProc al diccionario de listos
	pthread_mutex_lock(&mutex_general);
	pthread_mutex_lock(&mutex_ready);
	queue_push(cola_procesos_listos, (void*)pcb);
	log_info(log_planificador, "[%s] Entra a la cola de listos el mProc con PID: %d.",
				archivo_planificador->algortimo_planificacion, pcb->pid);
	sem_post(&cantidad_listos);
	pthread_mutex_lock(&mutex_respuesta);
	metrica * metrica1 = malloc(sizeof(metrica));
	metrica1->inicial = time(NULL);
	metrica1->total = 0;
	metrica * metrica2 = malloc(sizeof(metrica));
	metrica2->inicial = time(NULL);
	metrica2->total = 0;
	dictionary_put(tiempos_respuesta, string_itoa(pcb->pid), (void*)metrica2);
	dictionary_put(tiempos_espera, string_itoa(pcb->pid),(void*)metrica1);
	pthread_mutex_unlock(&mutex_respuesta);
	pthread_mutex_unlock(&mutex_ready);
	pthread_mutex_unlock(&mutex_general);

	pthread_t  h;
	pthread_create(&h, NULL,(void *) mandar_mProc_ejecutar, NULL);
}

void mandar_mProc_ejecutar() {
	pcb_t * pcb;

	//Espero que haya uno listo y alguna cpu libre para sacarlo de la cola
	sem_wait(&cantidad_listos);
	sem_wait(&cantidad_cpu_libres);

	sem_wait(&listo_ok);
	pthread_mutex_lock(&mutex_ready);
	pcb = queue_peek(cola_procesos_listos);
	pthread_mutex_unlock(&mutex_ready);

	cpu_t * cpu = get_cpu_libre();

	//Agrego cpu a la cola de cpu_ocupadas
	pthread_mutex_lock(&mutex_cpu_ocupadas);
	cpu->id_proceso_exec = pcb->pid;
	queue_push(cola_cpu_ocupadas,(void *)cpu);
	pthread_mutex_unlock(&mutex_cpu_ocupadas);

	int32_t quantum = 0;
	if(string_equals_ignore_case(archivo_planificador->algortimo_planificacion,"FIFO"))
		quantum = 0;
	else
		quantum = archivo_planificador->quantum;

	//Creo estructura contexto, la serializo y la mando a la cpu
	context_ejec * contexto = crear_contexto_ejecucion(pcb, quantum);
	char * contexto_serializado = serializar_contexto(contexto);
	char * mensaje = string_new();
	string_append(&mensaje, "N&");
	string_append(&mensaje, contexto_serializado);
	pthread_mutex_lock(&mutex_socket);
	enviar_string(cpu->fdCPU, mensaje);
	pthread_mutex_unlock(&mutex_socket);

	//Agrego proceso a cola de exec
	pthread_mutex_lock(&mutex_ready);
	pcb = queue_pop(cola_procesos_listos);
	log_info(log_planificador, "[%s] Sale de la cola de listos el mProc con PID: %d.",
			archivo_planificador->algortimo_planificacion, pcb->pid);
	sumar_tiempo_espera(pcb);
	pthread_mutex_unlock(&mutex_ready);

	pthread_mutex_lock(&mutex_exec);
	queue_push(cola_procesos_ejecutandose,(void*) pcb);
	log_info(log_planificador, "[%s] Entra a la cola de ejecutandose el mProc con PID: %d.",
			archivo_planificador->algortimo_planificacion, pcb->pid);
	sem_post(&cantidad_exec);
	pthread_mutex_unlock(&mutex_exec);

	sem_post(&listo_ok);
	//Libero estructuras
	free(contexto_serializado);
	free(mensaje);
	pthread_exit(NULL);
}


void actualizar_diccionario_espera(char * pid){

	metrica * aux = malloc(sizeof(metrica));
	pthread_mutex_lock(&mutex_espera);
	if(dictionary_has_key(tiempos_espera, pid)){
		metrica * m_espera = dictionary_remove(tiempos_espera, pid);
		aux->inicial = time(NULL);
		aux->total = m_espera->total;
		dictionary_put(tiempos_espera, pid, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_espera);
}

void sumar_tiempo_espera(pcb_t * pcb_new){
	pthread_mutex_lock(&mutex_espera);
	double diferenciaTotal = 0;
	int suma = 0;
	char * pid = string_itoa(pcb_new->pid);
	metrica * metrica_ready = malloc(sizeof(metrica));;
	if(dictionary_has_key(tiempos_espera, pid)){
		metrica_ready = dictionary_remove(tiempos_espera, pid);
		suma = metrica_ready->total;
		diferenciaTotal = difftime(time(NULL), metrica_ready->inicial);
	}
	int total = (int)diferenciaTotal;
	metrica_ready->total = total + suma;
	metrica_ready->inicial = NULL;
	dictionary_put(tiempos_espera, pid, (void*)metrica_ready);
	pthread_mutex_unlock(&mutex_espera);
}

int comando_correr(int argc, char* mensaje[]) {
	if (argc != 2) {
		puts("Uso: correr <mensaje>");
		return -1;
	}
	//if (!list_is_empty(cola_cpu_conectadas)) {
		pcb = crear_pcb(mensaje[1], "listo", pids, 0, 0);
		pids++;
		//Agrego proceso a la cola de listos
		agregar_pcb_cola_listos(pcb);
	//} else
		//puts("No hay CPU conectada");

	int ret = 1;
	return ret;
}

int comando_finalizar(int argc, char** mensaje){
	if (argc != 2) {
		puts("Uso: finalizar <mensaje>");
		return -1;
	}

	if (!queue_is_empty(cola_procesos_ejecutandose) ||
				!queue_is_empty(cola_procesos_listos) ||
				!queue_is_empty(cola_procesos_io) ||
				!queue_is_empty(cola_procesos_suspendidos)) {
			while(actualizar_program_counter_para_finalizar(atoi(mensaje[1])) == 0){};
	}
	else
		puts("No hay mProc para finalizar");
	return 0;

}

void run_to_finish(char * pcb_serializada, char * id_cpu, int32_t bandera, char * mensaje) {
	pcb_t * pcb = deserializar_pcb(pcb_serializada);
	buscar_pcb(pcb->pid);

	//Agrego la pcb a la cola de finalizados
	pthread_mutex_lock(&mutex_exit);
	queue_push(cola_procesos_finalizados,(void*) pcb);
	sem_post(&cantidad_finish);
	pthread_mutex_unlock(&mutex_exit);
	if(bandera == 1){
		mostrar_mensaje_rafaga(mensaje, pcb->pid);
		metrica * metrica2 = dictionary_get(tiempos_respuesta,string_itoa(pcb->pid));
		if(metrica2->total == 0 )
			sumar_tiempo_ejecucion(pcb, mutex_respuesta, tiempos_respuesta, 2);

		pthread_mutex_lock(&mutex_espera);
		metrica * metrica_espera = dictionary_get(tiempos_espera, string_itoa(pcb->pid));
		pthread_mutex_unlock(&mutex_espera);

		log_info(log_planificador,"--------------------------------------------------------------------");
		log_info(log_planificador, "[CPU ID: %s] Finalizó ejecucion de mProc PID: %d.",
				id_cpu, pcb->pid);
		log_info(log_planificador,
				"[METRICA] Tiempo de respuesta del mProc PID: %d fue de (segundos): %d.",
				pcb->pid, metrica2->total);

		log_info(log_planificador,
				"[METRICA] Tiempo de espera del mProc PID: %d fue de (segundos): %d.",
				pcb->pid, metrica_espera->total);

		sumar_tiempo_ejecucion(pcb, mutex_ejecucion, tiempos_ejecucion, 1);
		log_info(log_planificador,"--------------------------------------------------------------------");
	}
	else{
		log_error(log_planificador,
				"Error con el mProc PID: %d . Se aborta su ejecucion.", pcb->pid);
		log_info(log_planificador, "Entró a la cola de finalizados el mProc PID: %d", pcb->pid);
	}
}

void fin_quantum(cpu_t * aux_cpu, pcb_t * pcb_quantum, int32_t program_counter) {
	// Actualizacion pcb
	if(pcb->program_counter != (pcb->cant_var_cont_actual-1) &&
			!string_equals_ignore_case(pcb_quantum->estado, "F")){
		actualizar_pcb(pcb_quantum->pid, program_counter, 1);
	}

	pthread_mutex_lock(&mutex_get_pcb);
	pcb_t * aux_pcb = get_pcb_otros_exec(pcb_quantum->pid);
	pthread_mutex_unlock(&mutex_get_pcb);

	// Pone el pcb en ready
	pthread_mutex_lock(&mutex_ready);
	queue_push(cola_procesos_listos,(void*)aux_pcb);
	log_info(log_planificador, "[%s] Entra a la cola de listos el mProc con PID: %d.",
				archivo_planificador->algortimo_planificacion, aux_pcb->pid);
	actualizar_diccionario_espera(string_itoa(aux_pcb->pid));
	pthread_mutex_unlock(&mutex_ready);

	libero_cpu(aux_pcb->pid, pcb_quantum->retardo_io, 0);
	sem_post(&cantidad_block);
}

void proximo() {

	//Espero que haya cpus libres
	sem_wait(&cantidad_cpu_libres);
	cpu_t * cpu = get_cpu_libre();

	//Paso pcb a cola de exec
	pthread_mutex_lock(&mutex_ready);
	pcb_t * pcb = queue_peek(cola_procesos_listos);
	pthread_mutex_unlock(&mutex_ready);

	//Agrego cpu a la cola de ocupadas
	pthread_mutex_lock(&mutex_cpu_ocupadas);
	cpu->id_proceso_exec = pcb->pid;
	queue_push(cola_cpu_ocupadas, (void*)cpu);
	pthread_mutex_unlock(&mutex_cpu_ocupadas);

	int32_t quantum;
	if(string_equals_ignore_case(archivo_planificador->algortimo_planificacion,"FIFO"))
		quantum = 0;
	else
		quantum = archivo_planificador->quantum;

	char * mensaje = string_new();
	context_ejec * contexto = crear_contexto_ejecucion(pcb, quantum);
	char * serial = serializar_contexto(contexto);

	//Le envio a la cpu la pcb actualizada
	string_append(&mensaje,"N&");
	string_append(&mensaje, serial);
	pthread_mutex_lock(&mutex_socket);
	enviar_string(cpu->fdCPU, mensaje);
	pthread_mutex_unlock(&mutex_socket);

	//Ya mandado el mje la saco de la cola de listos y la pongo en la de ejecutados
	pasar_ready_to_exec(pcb);
	free(serial);
	free(mensaje);
}

void pasar_ready_to_exec(pcb_t * pcb){
	pthread_mutex_lock(&mutex_general);
	int i;
	pcb_t * aux_pcb_otros;
	pthread_mutex_lock(&mutex_ready);
	int tam = queue_size(cola_procesos_listos);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_listos);
		if (pcb->pid == aux_pcb_otros->pid){
			log_info(log_planificador, "[%s] Sale de la cola de listos el mProc con PID: %d.",
						archivo_planificador->algortimo_planificacion, aux_pcb_otros->pid);
			sumar_tiempo_espera(aux_pcb_otros);
			break;
		}
		queue_push(cola_procesos_listos,(void*) aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_ready);

	//sem_wait(&ejec_ok);
	pthread_mutex_lock(&mutex_exec);
	queue_push(cola_procesos_ejecutandose,(void *)aux_pcb_otros);
	log_info(log_planificador, "[%s] Entra a la cola de ejecutandose el mProc con PID: %d.",
							archivo_planificador->algortimo_planificacion, aux_pcb_otros->pid);
	sem_post(&cantidad_exec);
	pthread_mutex_unlock(&mutex_exec);
	pthread_mutex_unlock(&mutex_general);
}

void manejador_fin_quantum(){
	while(quit_sistema){
		//Espero que haya algun proceso susp por quantum para ponerlo en la cola de exec
		sem_wait(&cantidad_block);
		proximo();
		sem_post(&cantidad_exec);
	}
}

void pasar_io_to_ready(pcb_t * pcb_to_io) {
	int32_t pc = pcb_to_io->program_counter;
	//Agrego pcb a cola de listos
	if(pcb_to_io->program_counter != (pcb_to_io->cant_var_cont_actual-1) &&
			!string_equals_ignore_case(pcb_to_io->estado, "F")){
		pthread_mutex_lock(&mutex_ready);
		pcb_to_io->program_counter = pc + 1;
		pthread_mutex_unlock(&mutex_ready);
	}

	pthread_mutex_lock(&mutex_general);
	pthread_mutex_lock(&mutex_ready);
	pcb_to_io->retardo_io = 0;
	queue_push(cola_procesos_listos,(void *) pcb_to_io);
	log_info(log_planificador, "[%s] Entra a la cola de listos el mProc con PID: %d.",
				archivo_planificador->algortimo_planificacion, pcb_to_io->pid);
	actualizar_diccionario_espera(string_itoa(pcb_to_io->pid));
	sem_post(&cantidad_listos);
	pthread_mutex_unlock(&mutex_ready);
	pthread_mutex_unlock(&mutex_general);

	//Busco cpu por pid de la pcb
	sem_wait(&cantidad_cpu_libres);
	cpu_t * cpu = get_cpu_libre();

	pthread_mutex_lock(&mutex_ready);
	pcb_t * listo = queue_peek(cola_procesos_listos);
	pthread_mutex_unlock(&mutex_ready);

	pthread_mutex_lock(&mutex_cpu_ocupadas);
	cpu->id_proceso_exec = listo->pid;
	queue_push(cola_cpu_ocupadas, (void*)cpu);
	pthread_mutex_unlock(&mutex_cpu_ocupadas);

	int32_t quantum = 0;
	if(string_equals_ignore_case(archivo_planificador->algortimo_planificacion,"FIFO"))
		quantum = 0;
	else
		quantum = archivo_planificador->quantum;

	char * mensaje = string_new();
	context_ejec * contexto = crear_contexto_ejecucion(listo, quantum);
	char * serial = serializar_contexto(contexto);
	//Le envio a la cpu la pcb actualizada
	string_append(&mensaje,"N&");
	string_append(&mensaje, serial);
	pthread_mutex_lock(&mutex_socket);
	enviar_string(cpu->fdCPU, mensaje);
	pthread_mutex_unlock(&mutex_socket);

	pasar_ready_to_exec(listo);
	//free(serial);
	free(mensaje);
}

cpu_t * buscar_cpu(int32_t pid){
	int32_t i;
	cpu_t * aux;
	pthread_mutex_lock(&mutex_cpu_ocupadas);
	int32_t longi = queue_size(cola_cpu_ocupadas);
	for(i=0; i < longi; i++){
		aux = queue_pop(cola_cpu_ocupadas);
		if(aux->id_proceso_exec == pid){
			pthread_mutex_unlock(&mutex_cpus);
			return aux;
		}
		queue_push(cola_cpu_ocupadas, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_cpu_ocupadas);
	return NULL;
}

pcb_t * buscar_pcb_sin_sacarla(int32_t pid){
	int32_t i;
	pcb_t * aux;
	pthread_mutex_lock(&mutex_exec);
	int32_t longi = queue_size(cola_procesos_ejecutandose);
	for(i=0; i < longi; i++){
		aux = queue_peek(cola_procesos_ejecutandose);
		if(aux->pid == pid){
			pthread_mutex_unlock(&mutex_exec);
			return aux;
		}
		//queue_push(cola_procesos_ejecutandose, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_exec);
	return NULL;
}

void buscar_cpu_por_id(char * id){
	int32_t i;
	cpu_t * aux;
	pthread_mutex_lock(&mutex_cpu_ocupadas);
	int32_t longi = queue_size(cola_cpu_ocupadas);
	for(i=0; i < longi; i++){
		aux = queue_pop(cola_cpu_ocupadas);
		if(string_equals_ignore_case(aux->idCPU,id)){
			break;
		}
		queue_push(cola_cpu_ocupadas,(void*) aux);
	}
	pthread_mutex_unlock(&mutex_cpu_ocupadas);
}

cpu_t * buscar_cpu_por_pid(int32_t pid){
	int32_t i;
	cpu_t * aux;
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	int32_t longi = queue_size(cola_cpu_ocupadas);
	for(i=0; i < longi; i++){
		aux = queue_peek(cola_cpu_ocupadas);
		if(aux->id_proceso_exec == pid){
			pthread_mutex_unlock(&mutex_cpus);
			return aux;
		}
		//queue_push(cola_cpu_ocupadas, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	return NULL;
}

void libero_cpu(int32_t pid, int32_t retardo_io, int bandera){

	//Libero la cpu por ejecucion de entrada salida de mproc con pid -> pid
	int32_t i;
	cpu_t * aux;
	pthread_mutex_lock(&mutex_cpu_ocupadas);
	int32_t longi = queue_size(cola_cpu_ocupadas);
	for(i=0; i < longi; i++){
		aux = queue_pop(cola_cpu_ocupadas);
		if(aux->id_proceso_exec == pid){
			pthread_mutex_lock(&mutex_cpus);
			if(bandera == 1){
				log_info(log_planificador,
					"[CPU ID: %s] Va a inicar una E/S con retardo: %d el mProc PID: %d.",
					aux->idCPU, retardo_io, pid);
			}
			//La coloco en la cola de cpus disponibles
			aux->id_proceso_exec = 0;
			queue_push(cola_cpu,(void*) aux);
			sem_post(&cantidad_cpu_libres);
			pthread_mutex_unlock(&mutex_cpus);
			break;
		}
		queue_push(cola_cpu_ocupadas,(void*) aux);
	}
	pthread_mutex_unlock(&mutex_cpu_ocupadas);
}

void pasar_ejec_to_io(int32_t pid, int32_t retardo){
	int32_t i = 0;
	pcb_t * pcb_aux;

	pthread_mutex_lock(&mutex_exec);
	int32_t longitud = queue_size(cola_procesos_ejecutandose);
	for (i=0; i< longitud; i++){
		pcb_aux = queue_pop(cola_procesos_ejecutandose);
		if (pcb_aux->pid == pid){
			log_info(log_planificador, "[%s] Sale de la cola de ejecutandose el mProc con PID: %d.",
						archivo_planificador->algortimo_planificacion, pcb_aux->pid);
			//sem_post(&ejec_ok);
			break;
		}
		else
			queue_push(cola_procesos_ejecutandose,(void*) pcb_aux);
	}
	pthread_mutex_unlock(&mutex_exec);

	libero_cpu(pcb_aux->pid, pcb_aux->retardo_io, 1);

	pthread_mutex_lock(&mutex_io);
	pcb_aux->retardo_io = retardo;
	queue_push(cola_procesos_io, (void*)pcb_aux);
	log_info(log_planificador, "[%s] Entra a la cola de E/S el mProc con PID: %d.",
				archivo_planificador->algortimo_planificacion, pcb_aux->pid);
	pthread_mutex_unlock(&mutex_io);
	sem_post(&cantidad_io);
}

void manejador_IO(){
	pcb_t * aux;
	while(quit_sistema){
		sem_wait(&cantidad_io);
		pthread_mutex_lock(&mutex_io);
		aux = queue_peek(cola_procesos_io);
		pthread_mutex_unlock(&mutex_io);
		if(aux->program_counter != (aux->cant_var_cont_actual-1)&&
				!string_equals_ignore_case(aux->estado, "F")){
			usleep(aux->retardo_io*1000000);
		}
		pthread_mutex_lock(&mutex_respuesta);
		metrica * metrica_io = dictionary_get(tiempos_respuesta,string_itoa(pcb->pid));
		if(metrica_io->total == 0)
			sumar_tiempo_ejecucion(aux, mutex_respuesta, tiempos_respuesta, 2);
		pthread_mutex_unlock(&mutex_respuesta);
		buscar_pcb(aux->pid);
		pasar_io_to_ready(aux);
	}
}

void sumar_tiempo_ejecucion(pcb_t * pcb_new, pthread_mutex_t mutex, t_dictionary * diccionario, int32_t tipo){
	//pthread_mutex_lock(&mutex);
	double diferenciaTotal;
	char * pid = string_itoa(pcb_new->pid);
	metrica * metrica_fin;
	if(dictionary_has_key(diccionario, pid)){
		metrica_fin = dictionary_get(diccionario, pid);
		diferenciaTotal = difftime(time(NULL), metrica_fin->inicial);
		dictionary_remove(diccionario, pid);
	}
	int total = (int)diferenciaTotal;
	metrica_fin->total = total;
	dictionary_put(diccionario, pid, (void*)metrica_fin);
	if(tipo == 1){
	log_info(log_planificador,
			"[METRICA] El tiempo de ejecucion del mProc PID: %dfue de (segundos): %d." ,
			pcb_new->pid, metrica_fin->total);
	}
}

 cpu_t * get_cpu_libre() {

	cpu_t *cpu_final = NULL;
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	while (!queue_is_empty(cola_cpu)) {
		cpu_t * cpu = queue_pop(cola_cpu);
		if (cpu->id_proceso_exec == 0){
			cpu_final = cpu;
			break;
		}
		queue_push(cola_cpu,(void*) cpu);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	if(cpu_final == NULL)
		puts("No hay cpu disponible 1");
	return cpu_final;
}

void dar_de_baja_cpu(){
	int32_t i;
	cpu_t * cpu;
	pthread_mutex_lock(&mutex_uso_cola_cpu);
	int32_t tam = queue_size(cola_cpu);
	for(i=0 ;i < tam ;i++){
		cpu = queue_pop(cola_cpu);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);

	pthread_mutex_lock(&mutex_uso_cola_cpu);
	int32_t tam2 = queue_size(cola_cpu_ocupadas);
	for(i=0 ;i < tam2 ;i++){
		cpu = queue_pop(cola_cpu_ocupadas);
		//Busca una pcb por pid y la saca de la cola donde estaba
		pcb_t * pcb = buscar_pcb(cpu->id_proceso_exec);
		if(pcb != NULL)
			queue_push(cola_procesos_finalizados, (void *)pcb);
	}
	pthread_mutex_unlock(&mutex_uso_cola_cpu);
	destruir_estructura_cpu(cpu);
}

int actualizar_program_counter_para_finalizar(int32_t pid){

	pcb_t * aux_pcb_otros;
	int i;
	int fin = 0;
	//BUsco en cola de ejec
	pthread_mutex_lock(&mutex_general);
	pthread_mutex_lock(&mutex_exec);
	int32_t tam = queue_size(cola_procesos_ejecutandose);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_ejecutandose);
		if (pid == aux_pcb_otros->pid){
			aux_pcb_otros->estado = "F";
			aux_pcb_otros->program_counter = aux_pcb_otros->cant_var_cont_actual -1;
			pthread_mutex_unlock(&mutex_exec);
			fin = 1;
		}
		queue_push(cola_procesos_ejecutandose, (void*)aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);

	//Busco en cola de ready
	pthread_mutex_lock(&mutex_ready);
	tam = queue_size(cola_procesos_listos);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_listos);
		if (pid == aux_pcb_otros->pid){
			aux_pcb_otros->estado = "F";
			aux_pcb_otros->program_counter = aux_pcb_otros->cant_var_cont_actual -1;
			pthread_mutex_unlock(&mutex_ready);
			fin = 1;
		}
		queue_push(cola_procesos_listos, (void*)aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_ready);

	//Busco en cola block
	pthread_mutex_lock(&mutex_block);
	tam = queue_size(cola_procesos_suspendidos);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_suspendidos);
		if (pid == aux_pcb_otros->pid){
			aux_pcb_otros->estado = "F";
			aux_pcb_otros->program_counter = aux_pcb_otros->cant_var_cont_actual -1;
			pthread_mutex_unlock(&mutex_block);
			fin = 1;
		}
		queue_push(cola_procesos_suspendidos, (void*)aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_block);

	//Busco en cola de io
	pthread_mutex_lock(&mutex_io);
	tam = queue_size(cola_procesos_io);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_io);
		if (pid == aux_pcb_otros->pid){
			aux_pcb_otros->estado = "F";
			aux_pcb_otros->program_counter = aux_pcb_otros->cant_var_cont_actual -1;
			pthread_mutex_unlock(&mutex_io);
			fin = 1;
		}
		queue_push(cola_procesos_io,(void*) aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_io);
	pthread_mutex_unlock(&mutex_general);
	return fin;
}

pcb_t * buscar_pcb(int32_t pid){

	pcb_t * aux_pcb_otros;
	int i;

	//BUsco en cola de ejec
	pthread_mutex_lock(&mutex_exec);
	int32_t tam = queue_size(cola_procesos_ejecutandose);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_ejecutandose);
		if (pid == aux_pcb_otros->pid){
			log_info(log_planificador, "[%s] Sale de la cola de ejecutandose el mProc con PID: %d.",
									archivo_planificador->algortimo_planificacion, aux_pcb_otros->pid);
			pthread_mutex_unlock(&mutex_exec);
			//sem_post(&ejec_ok);
			return aux_pcb_otros;
		}
		queue_push(cola_procesos_ejecutandose,(void*) aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);

	//Busco en cola de ready
	pthread_mutex_lock(&mutex_ready);
	tam = queue_size(cola_procesos_listos);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_listos);
		if (pid == aux_pcb_otros->pid){
			log_info(log_planificador, "[%s] Sale de la cola de listos el mProc con PID: %d.",
						archivo_planificador->algortimo_planificacion, aux_pcb_otros->pid);
			sumar_tiempo_espera(aux_pcb_otros);
			pthread_mutex_unlock(&mutex_ready);
			return aux_pcb_otros;
		}
		queue_push(cola_procesos_listos,(void*) aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_ready);

	//Busco en cola de io
	pthread_mutex_lock(&mutex_io);
	tam = queue_size(cola_procesos_io);
	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_io);
		if (pid == aux_pcb_otros->pid){
			log_info(log_planificador, "[%s] Sale de la cola de E/S el mProc con PID: %d.",
					archivo_planificador->algortimo_planificacion, aux_pcb_otros->pid);
			pthread_mutex_unlock(&mutex_io);
			return aux_pcb_otros;
		}
		queue_push(cola_procesos_io, (void*)aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_io);
	return NULL;
}

void actualizar_pcb(int32_t pid, int32_t program_counter_nuevo, int32_t tipo_cola) {
	if(tipo_cola == 1){
		pthread_mutex_lock(&mutex_exec);
		int32_t longitud = queue_size(cola_procesos_ejecutandose);
		int i;
		for(i = 0; i< longitud; i++ ) {
			pcb_t * aux = queue_pop(cola_procesos_ejecutandose);
			if (aux->pid == pid){
				aux->program_counter = program_counter_nuevo;
			}
			queue_push(cola_procesos_ejecutandose, (void*)aux);
		}
		pthread_mutex_unlock(&mutex_exec);
	}
}

void actualizar_total_pcb(char * pid, int32_t total) {
	pthread_mutex_lock(&mutex_exec);
	int32_t longitud = queue_size(cola_procesos_ejecutandose);
	int i;
	for(i = 0; i< longitud; i++ ) {
		pcb_t * aux = queue_pop(cola_procesos_ejecutandose);
		if (aux->pid == atoi(pid)){
			aux->cant_var_cont_actual = total;
		}
		queue_push(cola_procesos_ejecutandose, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_exec);
}

pcb_t * get_pcb_otros_exec(int32_t id_proc){
	pcb_t * aux_pcb_otros;
	int32_t i;
	pthread_mutex_lock(&mutex_exec);
	int32_t tam = queue_size(cola_procesos_ejecutandose);

	for(i=0 ;i < tam ;i++){
		aux_pcb_otros = queue_pop(cola_procesos_ejecutandose);
		if (id_proc == aux_pcb_otros->pid){
			log_info(log_planificador, "[%s] Sale de la cola de ejecutandose el mProc con PID: %d.",
									archivo_planificador->algortimo_planificacion, aux_pcb_otros->pid);
			pthread_mutex_unlock(&mutex_exec);
			//sem_post(&ejec_ok);
			return aux_pcb_otros;
		}
		queue_push(cola_procesos_ejecutandose, (void*)aux_pcb_otros);
	}
	pthread_mutex_unlock(&mutex_exec);
	return NULL;
}

void mostrar_mensaje_rafaga(char * mensaje_serializado, int32_t pid){

	char ** separados = string_split(mensaje_serializado, "%");
	int i = 0;
	pthread_mutex_lock(&mutex_mensajes);
	char ** mje_final;
	log_info(log_planificador,"------------------------------------------------------------------------");
	while(!string_equals_ignore_case(separados[i], "F")){
		mje_final = string_split(separados[i], "|");
		if(string_equals_ignore_case(mje_final[0], "I"))
					log_info(log_planificador,"[mProc: %d] Inicio %d paginas.",pid, atoi(mje_final[1]));
		if(string_equals_ignore_case(mje_final[0], "L"))
			log_info(log_planificador,"[mProc: %d] Pagina %d leida '%s'.",pid, atoi(mje_final[1]), mje_final[2]);

		if(string_equals_ignore_case(mje_final[0], "E"))
					log_info(log_planificador,"[mProc: %d] Pagina %d escrita: '%s'.",pid, atoi(mje_final[2]), mje_final[1]);
		i++;
	}
	log_info(log_planificador,"------------------------------------------------------------------------");
	pthread_mutex_unlock(&mutex_mensajes);
	free(separados);
}

void *mostrar_colas() {
	pthread_mutex_lock(&mutex_new);
	pthread_mutex_lock(&mutex_ready);
	pthread_mutex_lock(&mutex_block);
	pthread_mutex_lock(&mutex_exec);
	pthread_mutex_lock(&mutex_exit);

	printf(BLANCO "-------------------------PROCESOS----------------------------------\n");
	imprimir_cola(cola_procesos_ejecutandose, "Ejecutando");
	imprimir_cola(cola_procesos_listos, "Listo");
	imprimir_cola(cola_procesos_finalizados, "Finalizado");
	imprimir_cola(cola_procesos_io, "En E/S");
	printf(BLANCO "-------------------------------------------------------------------\n");
	printf(MAGENTA);
	pthread_mutex_unlock(&mutex_new);
	pthread_mutex_unlock(&mutex_ready);
	pthread_mutex_unlock(&mutex_block);
	pthread_mutex_unlock(&mutex_exec);
	pthread_mutex_unlock(&mutex_exit);
	return NULL;
}

 void imprimir_cola(t_queue * cola, char * estado) {
	int32_t i;

	if (!queue_is_empty(cola)) {
		for (i = 0; i < queue_size(cola); i++) {
			pcb_t * aux = queue_pop(cola);
			if(string_equals_ignore_case(estado, "Ejecutando") && (i >= 1)) estado = "Listo";
			printf("mProc %d:  %s -> %s \n", aux->pid, aux->path, estado);
			queue_push(cola, (void *)aux);
		}
	}
}

void mandar_porcentaje_cpu(){

	if(list_is_empty(cola_cpu_disponibles))
		puts("No hay CPU coenctadas.");
	else{
		int i;
		int tam = list_size(cola_cpu_disponibles);
		for(i=0; i< tam ; i++){
			cpu_t * aux = list_get(cola_cpu_disponibles, i);
			enviar_string(aux->fdCPU, "P|");
		}
	}
}

void guardar_porcentaje_cpu(cpu_t * cpu, int32_t porcentaje_nuevo){
	int32_t i;
	pthread_mutex_lock(&mutex_cpus);
	for (i = 0; i < queue_size(cola_cpu); i++) {
		cpu_t * aux = queue_pop(cola_cpu);
		if (aux->idCPU == cpu->idCPU)
			aux->porcentaje_uso = porcentaje_nuevo;
		queue_push(cola_cpu, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_cpus);

	pthread_mutex_lock(&mutex_cpu_ocupadas);
	for (i = 0; i < queue_size(cola_cpu_ocupadas); i++) {
		cpu_t * aux = queue_pop(cola_cpu_ocupadas);
		if (aux->idCPU == cpu->idCPU)
			aux->porcentaje_uso = porcentaje_nuevo;
		queue_push(cola_cpu_ocupadas, (void*)aux);
	}
	pthread_mutex_unlock(&mutex_cpu_ocupadas);

}

void mostrar_porcentaje(){
	cpus_c = 0;
	int32_t i;
	cpu_t *aux;
	printf(BLANCO "-------------------------PORCENTAJE DE USO CPUS----------------------------------\n");
	if (!queue_is_empty(cola_cpu_ocupadas)) {
		for (i = 0; i < queue_size(cola_cpu_ocupadas); i++) {
			aux = queue_pop(cola_cpu_ocupadas);
			printf("CPU %s: %d '%'\n", aux->idCPU, aux->porcentaje_uso);
			queue_push(cola_cpu_ocupadas,(void*)aux);
		}
	}
	if (!queue_is_empty(cola_cpu)) {
		for (i = 0; i < queue_size(cola_cpu); i++) {
			aux = queue_pop(cola_cpu);
			printf("CPU %s: %d '%'\n", aux->idCPU, aux->porcentaje_uso);
			queue_push(cola_cpu,(void*)aux);
		}
	}
	printf("----------------------------------------------------------------------------------------\n");
	printf(MAGENTA);

}

void imprimir_cpu() {
	int32_t i;
	cpu_t *aux;
	pthread_mutex_lock(&mutex_cpus);
	printf(BLANCO "-------------------------CPUS LIBES----------------------------------\n");
	if (!queue_is_empty(cola_cpu)) {
		for (i = 0; i < queue_size(cola_cpu); i++) {
			aux = queue_pop(cola_cpu);
			printf("cpu id %s: id proceso %d cantidad i: %d \n", aux->idCPU, aux->id_proceso_exec,i);
			queue_push(cola_cpu, (void*)aux);
		}
	}
	printf(BLANCO "-------------------------------------------------------------------------\n");
	pthread_mutex_unlock(&mutex_cpus);
	printf(BLANCO "-------------------------CPUS OCUPADAS----------------------------------\n");
		if (!queue_is_empty(cola_cpu_ocupadas)) {

			for (i = 0; i < queue_size(cola_cpu_ocupadas); i++) {
				aux = queue_pop(cola_cpu_ocupadas);
				printf("cpu id %s: id proceso %d cantidad i: %d \n", aux->idCPU, aux->id_proceso_exec,i);
				queue_push(cola_cpu_ocupadas, (void*)aux);
			}

		}
	printf(BLANCO"-------------------------------------------------------------------------\n");
	printf(MAGENTA);
}

int ayuda(int argc, char *argv[]){
    char * texto_ayuda = string_new();
    string_append(&texto_ayuda, "Comandos disponibles:\n");
    string_append(&texto_ayuda, "correr <path> - Corre un mProg\n");
    string_append(&texto_ayuda, "finalizar <pid> - Fnaliza un mProc\n");
    string_append(&texto_ayuda, "ms - MUestra cpus\n");
    string_append(&texto_ayuda, "ps - Muestra estado de los procesos\n");
    string_append(&texto_ayuda, "cpu - Muestra listado el porcentaje de uso de las CPU \n");
    string_append(&texto_ayuda, "salir - Cierra el proceso Planificador\n");
    printf("%s", texto_ayuda);
    free(texto_ayuda);
    return 1;
}

t_dictionary * inicializar_diccionario_de_comandos() {
	t_dictionary * dic_de_comandos = dictionary_create();
	dictionary_put(dic_de_comandos, "correr", (void *) comando_correr);
	dictionary_put(dic_de_comandos, "finalizar", (void *) comando_finalizar);
	dictionary_put(dic_de_comandos, "ps", (void *) mostrar_colas);
	dictionary_put(dic_de_comandos, "ms", (void *) imprimir_cpu);
	dictionary_put(dic_de_comandos, "cpu", (void *) mandar_porcentaje_cpu);
	dictionary_put(dic_de_comandos, "ayuda", (void *) ayuda);
	dictionary_put(dic_de_comandos, "?", (void *) ayuda);
	return dic_de_comandos;
}

void cerrar_todo(t_dictionary* dic_de_comandos) {

	//Libero los diccionarios
	dictionary_destroy(dic_de_comandos);
	dictionary_destroy(dic_cpus);
	dictionary_destroy(tiempos_ejecucion);
	dictionary_destroy(tiempos_espera);
	dictionary_destroy(tiempos_respuesta);

	//Libero los semaforos
	pthread_mutex_destroy(&mutex_block);
	pthread_mutex_destroy(&mutex_cpu_ocupadas);
	pthread_mutex_destroy(&mutex_cpus);
	pthread_mutex_destroy(&mutex_dic_exec);
	pthread_mutex_destroy(&mutex_exec);
	pthread_mutex_destroy(&mutex_exit);
	pthread_mutex_destroy(&mutex_io);
	pthread_mutex_destroy(&mutex_new);
	pthread_mutex_destroy(&mutex_pcb);
	pthread_mutex_destroy(&mutex_ready);
	pthread_mutex_destroy(&mutex_uso_cola_cpu);
	pthread_mutex_destroy(&mutex_ejecucion);
	pthread_mutex_destroy(&mutex_respuesta);
	pthread_mutex_destroy(&mutex_espera);
	pthread_mutex_destroy(&mutex_general);
	pthread_mutex_destroy(&mutex_mensajes);

	//Libero colas de procesos
	queue_destroy(cola_procesos_finalizados);
	queue_destroy(cola_procesos_ejecutandose);
	queue_destroy(cola_procesos_listos);
	queue_destroy(cola_procesos_suspendidos);
	queue_destroy(cola_procesos_io);
	queue_destroy(cola_cpu_ocupadas);

	//Libero listas
	list_destroy(cola_cpu_conectadas);
	list_destroy(cola_cpu_disponibles);

	quit_sistema = 0;
}

int contar_strings_en_lista(char *string[]) {
	int i;
	for (i = 0; string[i] != NULL; i++)
		;
	return i;
}

void ejecutar_comando(char * comando, t_dictionary *dic_de_comandos) {
	char ** comando_separado = string_split(comando, " ");
	int cantidad_de_parametros = contar_strings_en_lista(comando_separado);
	if (dictionary_has_key(dic_de_comandos, comando_separado[0])) {
		t_comando cmd = dictionary_get(dic_de_comandos, comando_separado[0]);
		cmd(cantidad_de_parametros, comando_separado);
	} else
		printf("No se encontró el comando %s\n", comando_separado[0]);
	//Libero el espacio que ocupo el comando
	int i;
	for (i = 0; comando_separado[i] != NULL; i++)
		free(comando_separado[i]);
	free(comando_separado);
	return;
}

void obtener_comando(char ** buffer) {
	char x;
	while ((x = getchar()) != '\n') {
		char * x_a_string = string_repeat(x, 1); //convierto el char a string
		string_append(buffer, x_a_string);
		free(x_a_string);
	}
}

int lanzar_consola(void * config) {
	t_dictionary * dic_de_comandos = inicializar_diccionario_de_comandos();
	inicializar_diccionarios_y_colas();
	archivo_planificador = (planificador *)config;
	log_info(log_planificador, "Abriendo la consola ..!");
	puts("Proceso Planificador - Tipee 'ayuda' o '?' para más información");
	printf("%s", PROMPT);
	char * comando = string_new();
	obtener_comando(&comando);
	while (!string_equals_ignore_case(comando, "salir")) {

		//Si solamente aprietan enter, vuelvo a mostrar el promt
		if (string_equals_ignore_case(comando, "")) {
			printf("%s", PROMPT);
			obtener_comando(&comando);
			continue;
		}

		//Si no, ejecuto el comando
		ejecutar_comando(comando, dic_de_comandos);

		//Lo limpio
		free(comando);
		comando = string_new();

		//y pregunto el siguiente comando
		printf("%s", PROMPT);
		obtener_comando(&comando);
	}
	free(comando);
	log_info(log_planificador, "***************   FIN DEL PROCESO PLANIFICADOR  ***************\n\n");
	cerrar_todo(dic_de_comandos);
	pthread_exit(NULL);
	return 0;
}
