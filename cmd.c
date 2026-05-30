#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>

#define COMM 15
#define MAX_COMMANDS 5
#define QUANTUM 10

typedef struct {
	char *command;
	char *arg;
} Command;

typedef struct Process {
	char name[100];
	int burst;
	int id;
	int size;
	char state[15];     // "new", "ready", "terminated"
	struct Process *next;
} Process_t;

typedef struct Bloque {
	int direccionBase;
	Process_t *procesoAlojado;
	int tamano;
	bool estado;
	struct Bloque *siguiente;
} Bloque_t;

int catFunction(char *filename, int flag);
int cpFunction(char *fileNameS, char *fileNameD);
int rmFunction(char *filename);
int contains(char *command, char *list[COMM]);
void mkProcessFunction(char *name, int burst, int size, int id, Process_t **head);
Process_t* deleteByID(Process_t **head, int id);
Process_t* findByID(Process_t **head, int id);
void insertFInal(Process_t **head, Process_t *aux);
int getLastID(Process_t **head);
void lstProcessFunction(Process_t **head);
void stats(Process_t* list[], int turnaround[], int waiting[], int n);
int fcfsFunction(Process_t **head, Bloque_t **memoria);
Process_t* findShortest(Process_t **head);
int sjfFunction(Process_t **head, Bloque_t **memoria);
int rrFunction(Process_t **head, Bloque_t **memoria);
void allocFunction(Process_t **head, int id, char *estrategia, Bloque_t **memoria);
void freeFunction(Process_t **head, int id, Bloque_t **memoria);
void mstatusFunction(Bloque_t **memoria);
void compactFunction(Bloque_t **memoria, int memoriaTotal);

char *commands[COMM] = {"mycat", "mycp", "remove", "exit", "mkprocess", "lsprocess",
                         "fcfs", "sjf", "rr", "alloc", "free", "mstatus", "compact", "my_kill", "cd"};

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Uso: %s <memoria>\n", argv[0]);
		return 1;
	}

	int memoriaTotal = atoi(argv[1]);
	if (memoriaTotal <= 0) {
		printf("Error: el tamaño de memoria debe ser un entero positivo\n");
		return 1;
	}

	Bloque_t *memoria = malloc(sizeof(Bloque_t));
	memoria->direccionBase = 0;
	memoria->tamano = memoriaTotal;
	memoria->estado = false;
	memoria->procesoAlojado = NULL;
	memoria->siguiente = NULL;

	char opcion[100] = "trash";
	char *comando;
	char *archivo;
	char *pipe1;
	Process_t *head = NULL;

	while (1) {
		printf("GOD_shell> ");
		fgets(opcion, sizeof(opcion), stdin);
		
		opcion[strcspn(opcion, "\n")] = '\0';
		
		char opcionTemp[100];       
    	strcpy(opcionTemp, opcion); 

		char *args[10];
		int argc_cmd = 0;

		char *token = strtok(opcion, " ");

		while (token != NULL && argc_cmd < 10) {
			args[argc_cmd++] = token;
			token = strtok(NULL, " ");
		}

		if (argc_cmd == 0) continue;

		char *comando = args[0];

		char *pipe1 = (argc_cmd > 1) ? args[1] : NULL;
		char *archivo = (argc_cmd > 2) ? args[2] : NULL;

		if (strcmp(comando, "exit") == 0) {
			break;

		// --------------------------------------------------------
		// mkprocess <id> <burst> <size>
		// --------------------------------------------------------
		} else if (strcmp(comando, "mkprocess") == 0) {
			char *sizeStr = (argc_cmd > 3) ? args[3] : NULL;

			if (pipe1 == NULL || archivo == NULL || sizeStr == NULL) {
				printf("Uso: mkprocess <id> <burst> <size>\n");
				continue;
			}

			int id    = atoi(pipe1);
			int burst = atoi(archivo);
			int size  = atoi(sizeStr);

			if (id <= 0) {
				printf("Error: id debe ser un entero positivo\n");
			} else if (burst <= 0) {
				printf("Error: burst debe ser un entero positivo\n");
			} else if (size <= 0) {
				printf("Error: size debe ser un entero positivo\n");
			} else if (findByID(&head, id) != NULL) {
				printf("Error: ya existe un proceso con id %d\n", id);
			} else {
				char autoName[20];
				snprintf(autoName, sizeof(autoName), "P%d", id);
				mkProcessFunction(autoName, burst, size, id, &head);
			}

		// my_kill <id>
		} else if (strcmp(comando, "my_kill") == 0 && pipe1 != NULL) {
			int esNumero = 1;
			for (int i = 0; pipe1[i] != '\0'; i++) {
				if (!isdigit((unsigned char)pipe1[i]) && !(i == 0 && pipe1[i] == '-')) {
					esNumero = 0;
					break;
				}
			}
			if (!esNumero) {
				printf("Error: id debe ser un entero\n");
			} else {
				int id = atoi(pipe1);
				Process_t *proc = findByID(&head, id);
				if (proc == NULL) {
					printf("Process %d not found\n", id);
				} else {
					if (strcmp(proc->state, "ready") == 0)
						freeFunction(&head, id, &memoria);
					Process_t *killed = deleteByID(&head, id);
					if (killed != NULL) {
						printf("Process %d (%s) removed\n", killed->id, killed->name);
						free(killed);
					}
				}
			}

		} else if (strcmp(comando, "my_kill") == 0 && pipe1 == NULL) {
			printf("Uso: my_kill <id>\n");

		} else if (strcmp(comando, "lsprocess") == 0) {
			lstProcessFunction(&head);

		} else if (strcmp(comando, "fcfs") == 0) {
			fcfsFunction(&head, &memoria);

		} else if (strcmp(comando, "sjf") == 0) {
			sjfFunction(&head, &memoria);

		} else if (strcmp(comando, "rr") == 0) {
			rrFunction(&head, &memoria);

		} else if (strcmp(comando, "alloc") == 0) {
			if (pipe1 == NULL || archivo == NULL) {
				printf("Uso: alloc <id> <estrategia>\n");
			} else {
				int id = atoi(pipe1);
				allocFunction(&head, id, archivo, &memoria);
			}

		} else if (strcmp(comando, "free") == 0) {
			if (pipe1 == NULL) {
				printf("Uso: free <id>\n");
			} else {
				int id = atoi(pipe1);
				freeFunction(&head, id, &memoria);
			}

		} else if (strcmp(comando, "mstatus") == 0) {
			mstatusFunction(&memoria);

		} else if (strcmp(comando, "compact") == 0) {
			compactFunction(&memoria, memoriaTotal);

		} else if (strcmp(comando, "mycat") == 0 && pipe1 != NULL) {
			if (strcmp(pipe1, ">") == 0 && archivo != NULL)
				catFunction(archivo, 0);
			else
				catFunction(pipe1, 1);

		} else if (strcmp(comando, "mycp") == 0 && archivo != NULL) {
			if (cpFunction(pipe1, archivo) == -1) printf("Error\n");

		} else if (strcmp(comando, "remove") == 0 && pipe1 != NULL) {
			if (rmFunction(pipe1) == -1) printf("Error\n");

		} else if (strcmp(comando, "cd") == 0) {
			if (pipe1 == NULL) {
				printf("Use: cd <directory>\n");
			} else if (chdir(pipe1) != 0) {
				perror("Error changing directory");
			}
		} else if (contains(comando, commands)) {
			char *elements[MAX_COMMANDS];
			char *element = strtok(opcionTemp, "|");
			int numElements = 0;
			while (element != NULL && numElements < MAX_COMMANDS) {
				elements[numElements] = element;
				++numElements;
				element = strtok(NULL, "|");
			}

			// Verificar si hay más comandos de los permitidos
			if (element != NULL) {
				printf("Error: maximo %d comandos en pipe\n", MAX_COMMANDS);
				continue;
			}

			int prev_fd = -1;
			for (int i = 0; i < numElements; ++i) {
				int pipes[2];
				if (i < numElements - 1) {
					if (pipe(pipes) == -1) {
						printf("error al crear el pipe %d\n", i);
						break;
					}
				}
				pid_t pid = fork();
				if (pid == 0) {
					if (prev_fd != -1) {
						dup2(prev_fd, STDIN_FILENO);
						close(prev_fd);
					}
					if (i < numElements - 1) {
						close(pipes[0]);
						dup2(pipes[1], STDOUT_FILENO);
						close(pipes[1]);
					}

					char segmento[100];                    
					strncpy(segmento, elements[i], 99);
					segmento[99] = '\0';

					char *args_exec[10];
					int n_args = 0;
					char *tok = strtok(segmento, " \t\n");   
					while (tok != NULL && n_args < 9) {
						args_exec[n_args++] = tok;
						tok = strtok(NULL, " \t\n");
					}
					args_exec[n_args] = NULL;

					execvp(args_exec[0], args_exec);
					char *args_tem[] = {"bash", "-c", args_exec[0], NULL};
					execvp("bash", args_tem);
					exit(127);
				}
				if (prev_fd != -1) close(prev_fd);
				if (i < numElements - 1) {
					close(pipes[1]);
					prev_fd = pipes[0];
				}
			}
			for (int i = 0; i < numElements; ++i) wait(NULL);

		} else {
			printf("Invalid command\n");
		}
	}
	return 0;
}

// ============================================================
// FUNCIONES DE PROCESOS
// ============================================================

void mkProcessFunction(char *name, int burst, int size, int id, Process_t **head) {
	Process_t *node = malloc(sizeof(Process_t));
	strncpy(node->name, name, sizeof(node->name) - 1);
	node->name[sizeof(node->name) - 1] = '\0';
	node->burst = burst;
	node->id = id;
	node->size = size;
	strcpy(node->state, "new");
	node->next = NULL;
	insertFInal(head, node);
	printf("Process %s created with id %d state new\n", name, id);
}

Process_t* findByID(Process_t **head, int id) {
	Process_t *rec = *head;
	while (rec != NULL) {
		if (rec->id == id) return rec;
		rec = rec->next;
	}
	return NULL;
}

Process_t* deleteByID(Process_t **head, int id) {
	if (*head == NULL) return NULL;
	if ((*head)->id == id) {
		Process_t *temp = *head;
		*head = (*head)->next;
		return temp;
	}
	Process_t *rec = *head;
	while (rec->next != NULL) {
		if (rec->next->id == id) {
			Process_t *temp = rec->next;
			rec->next = temp->next;
			return temp;
		}
		rec = rec->next;
	}
	return NULL;
}

void insertFInal(Process_t **head, Process_t *aux) {
	if (*head == NULL) { *head = aux; return; }
	Process_t *rec = *head;
	while (rec->next != NULL) rec = rec->next;
	rec->next = aux;
}

int getLastID(Process_t **head) {
	if (*head == NULL) return 0;
	Process_t *rec = *head;
	while (rec->next != NULL) rec = rec->next;
	return rec->id;
}

void lstProcessFunction(Process_t **head) {
    if (*head == NULL) { printf("Empty list\n"); return; }
    Process_t *rec = *head;
    printf("ID | NAME | BURST | BLOCKS | STATE\n");
    while (rec != NULL) {
        printf("%d %s %d %d %s\n",
            rec->id,
            rec->name,
            rec->burst,
            rec->size,   
            rec->state);
        rec = rec->next;
    }
}

// ============================================================
// ALGORITMOS
// ============================================================

int fcfsFunction(Process_t **head, Bloque_t **memoria) {
	Process_t *check = *head;
	int hayReady = 0;
	while (check != NULL) {
		if (strcmp(check->state, "ready") == 0) { hayReady = 1; break; }
		check = check->next;
	}
	if (!hayReady) { printf("No processes in ready state. Use alloc first.\n"); return 0; }

	int acum = 0, idx = 0;
	int turnaround[100], waiting[100];
	Process_t *list[100];
	Process_t *copy_head = NULL;

	Process_t *rec = *head;
	while (rec != NULL) {
		if (strcmp(rec->state, "ready") == 0) {
			Process_t *node = malloc(sizeof(Process_t));
			*node = *rec;
			node->next = NULL;
			insertFInal(&copy_head, node);
		}
		rec = rec->next;
	}

	Process_t *cur = copy_head;
	while (cur != NULL) {
		list[idx] = cur;
		printf("[%d] entra %s\n", acum, cur->name);
		waiting[idx] = acum;
		acum += cur->burst;
		turnaround[idx] = acum;
		printf("[%d] sale %s\n", acum, cur->name);

		Process_t *orig = findByID(head, cur->id);
		if (orig != NULL) {
			
			freeFunction(head, orig->id, memoria);
			strcpy(orig->state, "terminated");
		}

		idx++;
		cur = cur->next;
	}

	stats(list, turnaround, waiting, idx);
	return 0;
}

Process_t* findShortest(Process_t **head) {
	if (head == NULL || *head == NULL) return NULL;
	Process_t *shortest = NULL;
	Process_t *rec = *head;
	while (rec != NULL) {
		if (strcmp(rec->state, "ready") == 0) {
			if (shortest == NULL || rec->burst < shortest->burst ||
			   (rec->burst == shortest->burst && rec->id < shortest->id))
				shortest = rec;
		}
		rec = rec->next;
	}
	return shortest;
}

int sjfFunction(Process_t **head, Bloque_t **memoria) {
	Process_t *check = *head;
	int hayReady = 0;
	while (check != NULL) {
		if (strcmp(check->state, "ready") == 0) { hayReady = 1; break; }
		check = check->next;
	}
	if (!hayReady) { printf("No processes in ready state. Use alloc first.\n"); return 0; }

	int acum = 0, idx = 0;
	int turnaround[100], waiting[100];
	Process_t *list[100];

	while (1) {
		Process_t *node = findShortest(head);
		if (node == NULL) break;

		Process_t *copy = malloc(sizeof(Process_t));
		*copy = *node;
		copy->next = NULL;
		list[idx] = copy;

		printf("[%d] entra %s\n", acum, node->name);
		waiting[idx] = acum;
		acum += node->burst;
		turnaround[idx] = acum;
		printf("[%d] sale %s\n", acum, node->name);

		freeFunction(head, node->id, memoria);
		strcpy(node->state, "terminated");
		idx++;
	}

	stats(list, turnaround, waiting, idx);
	return 0;
}

int rrFunction(Process_t **head, Bloque_t **memoria) {
	Process_t *check = *head;
	int hayReady = 0;
	while (check != NULL) {
		if (strcmp(check->state, "ready") == 0) { hayReady = 1; break; }
		check = check->next;
	}
	if (!hayReady) { printf("No processes in ready state. Use alloc first.\n"); return 0; }

	Process_t *copy_head = NULL;
	int original_burst[100];
	int original_ids[100];
	Process_t *rec = *head;
	int n = 0;

	while (rec != NULL) {
		if (strcmp(rec->state, "ready") == 0) {
			Process_t *node = malloc(sizeof(Process_t));
			*node = *rec;
			node->next = NULL;
			original_burst[n] = rec->burst;
			original_ids[n] = rec->id;
			n++;
			insertFInal(&copy_head, node);
		}
		rec = rec->next;
	}

	int acum = 0, idx = 0;
	int turnaround[100], waiting[100];
	Process_t *list[100];

	while (copy_head != NULL) {
		Process_t *current = copy_head;
		copy_head = copy_head->next;
		current->next = NULL;

		printf("[%d] entra %s\n", acum, current->name);

		if (current->burst > QUANTUM) {
			acum += QUANTUM;
			current->burst -= QUANTUM;
			printf("[%d] sale %s (remaining: %d)\n", acum, current->name, current->burst);
			insertFInal(&copy_head, current);
		} else {
			acum += current->burst;
			printf("[%d] sale %s\n", acum, current->name);
			turnaround[idx] = acum;

			for (int i = 0; i < n; i++) {
				if (original_ids[i] == current->id) {
					waiting[idx] = turnaround[idx] - original_burst[i];
					current->burst = original_burst[i];
					break;
				}
			}

			Process_t *orig = findByID(head, current->id);
			if (orig != NULL) {
				freeFunction(head, orig->id, memoria);
				strcpy(orig->state, "terminated");
			}

			list[idx] = current;
			idx++;
		}
	}

	stats(list, turnaround, waiting, idx);
	return 0;
}

void stats(Process_t* list[], int turnaround[], int waiting[], int n) {
	printf("Process | Burst | Waiting | Turnaround\n");
	float avg_waiting = 0, avg_turnaround = 0;
	for (int i = 0; i < n; ++i) {
		if (list[i] != NULL) {
			printf("%s %d %d %d\n", list[i]->name, list[i]->burst, waiting[i], turnaround[i]);
			avg_waiting += waiting[i];
			avg_turnaround += turnaround[i];
			free(list[i]);
		}
	}
	if (n > 0) {
		printf("================================\n");
		printf("Average Waiting Time: %.2f\n", avg_waiting / n);
		printf("Average Turnaround Time: %.2f\n", avg_turnaround / n);
	}
}

// ============================================================
// FUNCIONES DE MEMORIA
// ============================================================

void allocFunction(Process_t **head, int id, char *estrategia, Bloque_t **memoria) {
	Process_t *proc = findByID(head, id);
	if (proc == NULL) { printf("Process %d not found\n", id); return; }

	if (strcmp(proc->state, "terminated") == 0) {
		printf("Process %d already terminated, cannot be allocated again\n", id);
		return;
	}
	if (strcmp(proc->state, "ready") == 0) {
		printf("Process %d is already in memory (ready)\n", id);
		return;
	}
	if (strcmp(proc->state, "new") != 0) {
		printf("Process %d is not in new state\n", id);
		return;
	}
	if (proc->size <= 0) {
		printf("Process %d has invalid size (%d), cannot allocate\n", id, proc->size);
		return;
	}
	char est[20];
	strncpy(est, estrategia, sizeof(est) - 1);
	est[sizeof(est) - 1] = '\0';
	for (int i = 0; est[i]; i++) est[i] = tolower((unsigned char)est[i]);

	int tamano = proc->size;
	Bloque_t *bloque = NULL;

	if (strcmp(est, "firstfit") == 0) {
		Bloque_t *actual = *memoria;
		while (actual != NULL) {
			if (!actual->estado && actual->tamano >= tamano) {
				bloque = actual;
				break;
			}
			actual = actual->siguiente;
		}
	} else if (strcmp(est, "worstfit") == 0) {
		Bloque_t *actual = *memoria;
		while (actual != NULL) {
			if (!actual->estado && actual->tamano >= tamano)
				if (bloque == NULL || actual->tamano > bloque->tamano)
					bloque = actual;
			actual = actual->siguiente;
		}
	} else if (strcmp(est, "bestfit") == 0) {
		Bloque_t *actual = *memoria;
		while (actual != NULL) {
			if (!actual->estado && actual->tamano >= tamano)
				if (bloque == NULL || actual->tamano < bloque->tamano)
					bloque = actual;
			actual = actual->siguiente;
		}
	} else {
		printf("Estrategia invalida. Usa: firstfit, worstfit, bestfit\n");
		return;
	}

	if (bloque == NULL) {
		printf("No hay espacio disponible para proceso %d (size: %d)\n", id, tamano);
		return;
	}

	int sobrante = bloque->tamano - tamano;
	if (sobrante > 0) {
		Bloque_t *nuevo = malloc(sizeof(Bloque_t));
		nuevo->direccionBase = bloque->direccionBase + tamano;
		nuevo->tamano = sobrante;
		nuevo->estado = false;
		nuevo->procesoAlojado = NULL;
		nuevo->siguiente = bloque->siguiente;
		bloque->siguiente = nuevo;
	}

	bloque->tamano = tamano;
	bloque->estado = true;
	bloque->procesoAlojado = proc;
	strcpy(proc->state, "ready");
	printf("Process %d loaded to memory at base %d state ready\n", id, bloque->direccionBase);
}

void freeFunction(Process_t **head, int id, Bloque_t **memoria) {
	Process_t *proc = findByID(head, id);
	if (proc == NULL) { printf("Process %d not found\n", id); return; }

	if (strcmp(proc->state, "terminated") == 0) {
		printf("Process %d already terminated and not in memory\n", id);
		return;
	}

	Bloque_t *actual = *memoria;
	while (actual != NULL) {
		if (actual->estado && actual->procesoAlojado != NULL &&
		    actual->procesoAlojado->id == id) {
			if (strcmp(actual->procesoAlojado->state, "terminated") != 0)
				strcpy(actual->procesoAlojado->state, "new");
			actual->estado = false;
			actual->procesoAlojado = NULL;
			printf("Process %d freed from memory\n", id);
			return;
		}
		actual = actual->siguiente;
	}
	printf("Process %d not found in memory\n", id);
}

void mstatusFunction(Bloque_t **memoria) {
	Bloque_t *actual = *memoria;
	printf("~~~ Memory Status ~~~\n");
	while (actual != NULL) {
		printf("Base: %d | Limit: %d | Size: %d | ",
			actual->direccionBase,
			actual->direccionBase + actual->tamano - 1,
			actual->tamano);
		if (actual->estado)
			printf("Occupied by: %s\n", actual->procesoAlojado->name);
		else
			printf("Free\n");
		actual = actual->siguiente;
	}
	printf("~~~~~~~~~~~~~~~~~~~~~\n");
}

void compactFunction(Bloque_t **memoria, int memoriaTotal) {
	int direccion = 0;
	Bloque_t *actual = *memoria;
	while (actual != NULL) {
		if (actual->estado) {
			actual->direccionBase = direccion;
			direccion += actual->tamano;
		}
		actual = actual->siguiente;
	}

	Bloque_t **ptr = memoria;
	while (*ptr != NULL) {
		if (!(*ptr)->estado) {
			Bloque_t *temp = *ptr;
			*ptr = (*ptr)->siguiente;
			free(temp);
		} else {
			ptr = &(*ptr)->siguiente;
		}
	}

	int memoriaUsada = 0;
	actual = *memoria;
	Bloque_t *ultimo = NULL;
	while (actual != NULL) {
		memoriaUsada += actual->tamano;
		ultimo = actual;
		actual = actual->siguiente;
	}

	int libreRestante = memoriaTotal - memoriaUsada;
	if (libreRestante > 0) {
		Bloque_t *libre = malloc(sizeof(Bloque_t));
		libre->direccionBase = memoriaUsada;
		libre->tamano = libreRestante;
		libre->estado = false;
		libre->procesoAlojado = NULL;
		libre->siguiente = NULL;
		if (ultimo != NULL)
			ultimo->siguiente = libre;
		else
			*memoria = libre;
	}

	printf("Memory compacted\n");
}

// ============================================================
// FUNCIONES DE ARCHIVO Y UTILIDADES
// ============================================================

int contains(char *command, char *list[COMM]) {
	for (int i = 0; i < COMM; ++i) {
		if (strcmp(command, list[i]) == 0) return 0;
	}
	return 1;
}

int catFunction(char *filename, int flag) {
	int id;
	if (flag == 1) {
		id = open(filename, O_RDONLY);
		if (id == -1) return -1;
		char buffer[1000];
		ssize_t n;
		while ((n = read(id, buffer, sizeof(buffer))) > 0)
			write(STDOUT_FILENO, buffer, n);
		close(id);
		return 1;
	} else {
		id = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (id == -1) return -1;
		char buffer[1000];
		ssize_t n;
		while ((n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1)) > 0) {
			buffer[n] = '\0';
			if (buffer[0] == '$' && buffer[1] == '\n') break;
			write(id, buffer, n);
		}
		close(id);
		return 1;
	}
}

int cpFunction(char *fileNameS, char *fileNameD) {
	int idS = open(fileNameS, O_RDONLY);
	if (idS == -1) return -1;
	int idD = open(fileNameD, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (idD == -1) return -1;
	char buffer[4096];
	ssize_t n;
	while ((n = read(idS, buffer, sizeof(buffer))) > 0) {
		ssize_t total = 0;
		while (total < n) {
			ssize_t escrito = write(idD, buffer + total, n - total);
			if (escrito == -1) return -1;
			total += escrito;
		}
	}
	close(idS);
	close(idD);
	return 1;
}

int rmFunction(char *filename) {
	return unlink(filename) == 0 ? 1 : -1;
}