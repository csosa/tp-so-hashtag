#include "serializadores.h"

pcb_t * crear_pcb(char * ruta, char * estado, int32_t pid, int32_t pc, int32_t cantidad_proc){
	pcb_t * pcb = malloc(sizeof(pcb_t));
	pcb->path = strdup(ruta);
	pcb->estado = strdup(estado);
	pcb->retardo_io = 0;
	if(pc != -1){
		pcb->program_counter = pc;
		pcb->cant_var_cont_actual = cantidad_proc;
	}
	else{
		pcb->program_counter = 0;
		pcb->cant_var_cont_actual = 0;
	}

	if(pid != -1)
		pcb->pid = pid;
	else
		pcb->pid = rand();

	return pcb;
}

context_ejec * crear_contexto_ejecucion(pcb_t * pcb, int32_t quantum){
	context_ejec * contexto = malloc(sizeof(context_ejec));
	contexto->pcb = pcb;
	contexto->quantum = quantum;
	return contexto;
}

cpu_t * crear_estructura_cpu_nueva(int32_t id){
	cpu_t * cpu = malloc(sizeof(cpu_t));
	cpu->fdCPU = -1;
	cpu->idCPU = strdup(string_itoa(id));
	cpu->id_proceso_exec = 0;
	cpu->porcentaje_uso = 0;
	return cpu;
}

cpu_t * crear_estructura_cpu(int32_t fd, char * id){
	cpu_t * cpu = malloc(sizeof(cpu_t));
	cpu->fdCPU = fd;
	cpu->idCPU = id;
	cpu->id_proceso_exec = 0;
	cpu->porcentaje_uso = 0;
	return cpu;
}

void destruir_estructura_pcb(pcb_t * pcb){
	free(pcb->estado);
	free(pcb->path);
	free(pcb);
}

void destruir_estructura_cpu(cpu_t * cpu){
	free(cpu->idCPU);
	free(cpu);
}

void destruir_estructura_contexto(context_ejec * contexto){
	destruir_estructura_pcb(contexto->pcb);
	free(contexto);
}

char * serializar_pcb(pcb_t * pcb){
	char *pcb_serializado = string_new();

	string_append(&pcb_serializado, pcb->estado);
	string_append(&pcb_serializado, "|");
	string_append(&pcb_serializado, pcb->path);
	string_append(&pcb_serializado, "|");

	char *pid_string = string_new();
	pid_string = string_itoa(pcb->pid);
	string_append(&pcb_serializado, pid_string);
	string_append(&pcb_serializado, "|");
	string_append(&pcb_serializado, string_itoa(pcb->cant_var_cont_actual));
	string_append(&pcb_serializado, "|");
	string_append(&pcb_serializado, string_itoa(pcb->program_counter));

	free(pid_string);
	return pcb_serializado;
}

char * serializar_cpu(cpu_t * cpu){
	char *cpu_serializada = string_new();

	string_append(&cpu_serializada, string_itoa(cpu->fdCPU));
	string_append(&cpu_serializada,"|");
	string_append(&cpu_serializada, cpu->idCPU);

	return cpu_serializada;
}

char * serializar_contexto(context_ejec *contexto){
	char *contexto_serializado = string_new();
	char *pcb_serializado = string_new();

	pcb_serializado = serializar_pcb(contexto->pcb);
	string_append(&contexto_serializado, pcb_serializado);
	string_append(&contexto_serializado,"|");

	char *quantum_string = string_new();
	quantum_string = string_itoa(contexto->quantum);
	string_append(&contexto_serializado, quantum_string);

	free(quantum_string);
	free(pcb_serializado);
	return contexto_serializado;
}

pcb_t * deserializar_pcb(char *pcb_serializado){

	char **datos_pcb = string_split(pcb_serializado,"|");
	char *estado = datos_pcb[0];
	char *path = datos_pcb[1];
	char *pid = datos_pcb[2];
	char * cant_var_proc_exec = datos_pcb[3];
	char * program_counter = datos_pcb[4];
	pcb_t *pcb = crear_pcb(path, estado, atoi(pid), atoi(program_counter), atoi(cant_var_proc_exec));

	//Libero memoria
	free(datos_pcb);
	free(estado);
	free(pid);
	free(program_counter);
	free(cant_var_proc_exec);

	//Retorno la pcb
	return pcb;
}

cpu_t * deserializar_cpu(char * cpu_serializada){
	char **datos_cpu = string_split(cpu_serializada,"|");
	char *indicador = datos_cpu[0];
	char * fdCPU = datos_cpu[1];
	char * id = datos_cpu[2];
	cpu_t * cpu = crear_estructura_cpu(atoi(fdCPU), id);

	//Libero memoria
	free(datos_cpu);
	free(indicador);
	free(fdCPU);

	//Retorno la pcb
	return cpu;
}

context_ejec * deserializar_contexto(char *contexto_serializado){
	char ** datos_contexto = string_split(contexto_serializado,"|");
	char * estado = strdup(datos_contexto[0]);
	char * path = strdup(datos_contexto[1]);
	int32_t pid = atoi(datos_contexto[2]);
	int32_t pc = atoi(datos_contexto[4]);
	int32_t cantidad = atoi(datos_contexto[3]);
	pcb_t * pcb = crear_pcb(path, estado, pid, pc, cantidad);
	context_ejec * contexto = crear_contexto_ejecucion(pcb, atoi(datos_contexto[5]));

	free(datos_contexto);
	free(estado);
	return contexto;
}

void * agregar_elemento_a_diccionario(t_dictionary * dict, char * key,
		void * value, pthread_mutex_t * mutex) {
	void * ret = NULL;
	if (dictionary_has_key(dict, key))
		ret = quitar_elemento_a_diccionario(dict, key, &mutex);
	pthread_mutex_lock(&mutex);
	dictionary_put(dict, key, value);
	pthread_mutex_unlock(&mutex);
	return ret;
}

void * quitar_elemento_a_diccionario(t_dictionary * dict, char * key,
		pthread_mutex_t * mutex) {
	void * ret = NULL;
	pthread_mutex_lock(&mutex);
	if (dictionary_has_key(dict, key))
		ret = dictionary_remove(dict, key);
	pthread_mutex_unlock(&mutex);
	return ret;
}

void sem_decre(sem_t *sem){

	if (sem_wait(sem) == -1){
		perror("ERROR sem decrementar");
		exit(1);
	}
}

void sem_incre(sem_t *sem){

	if (sem_post(sem) == -1){
		perror("ERROR sem decrementar");
		exit(1);
	}
}

void crear_cont(sem_t *sem ,int32_t val_ini){

	if(sem_init(sem, 0, val_ini)== -1){
		perror("No se puede crear el semáforo");
		exit(1);
	}
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
