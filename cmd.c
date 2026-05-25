#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>

#define COMM 14
#define MAX_COMMANDS 10
#define QUANTUM 10

typedef struct {
	char *command;
	char *arg;
} Command;

typedef struct Process {
	char name[100];
	int burst;
	int id;
	int size;           // bloques de memoria
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
void compactFunction(Bloque_t **memoria);

char *commands[COMM] = {"mycat", "mycp", "remove", "exitt", "mkprocess", "lsprocess",
                         "fcfs", "sjf", "rr", "alloc", "free", "mstatus", "compact", "my_kill"};

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Uso: %s <memoria>\n", argv[0]);
		return 1;
	}

	int memoriaTotal = atoi(argv[1]);

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
		printf(">");
		fgets(opcion, sizeof(opcion), stdin);
		char opcionTemp[100];
		strcpy(opcionTemp, opcion);
		opcion[strcspn(opcion, "\n")] = '\0';
		comando = strtok(opcion, " ");

		if (comando == NULL) continue;

		pipe1 = strtok(NULL, " ");
		archivo = strtok(NULL, " ");

		if (archivo == NULL) {
			archivo = pipe1;
		}

		if (strcmp(comando, "exitt") == 0) {
			break;

		// --------------------------------------------------------
		// mkprocess <id> <burst> <size>
		// --------------------------------------------------------
		} else if (strcmp(comando, "mkprocess") == 0 && pipe1 != NULL && archivo != NULL) {
			char *sizeStr = strtok(NULL, " ");
			int id    = atoi(pipe1);
			int burst = atoi(archivo);
			int size  = (sizeStr != NULL) ? atoi(sizeStr) : 0;

			// Validar ID duplicado
			if (findByID(&head, id) != NULL) {
				printf("Error: ya existe un proceso con id %d\n", id);
			} else {
				char autoName[20];
				snprintf(autoName, sizeof(autoName), "P%d", id);
				mkProcessFunction(autoName, burst, size, id, &head);
			}

		// --------------------------------------------------------
		// my_kill <id>  — free interno si está en ready
		// --------------------------------------------------------
		} else if (strcmp(comando, "my_kill") == 0 && archivo != NULL) {
			int id = atoi(archivo);
			Process_t *proc = findByID(&head, id);
			if (proc == NULL) {
				printf("Process %d not found\n", id);
			} else {
				// Si está en ready, liberarlo de memoria primero
				if (strcmp(proc->state, "ready") == 0) {
					freeFunction(&head, id, &memoria);
				}
				Process_t *killed = deleteByID(&head, id);
				if (killed != NULL) {
					printf("Process %d (%s) removed\n", killed->id, killed->name);
					free(killed);
				}
			}

		} else if (strcmp(comando, "lsprocess") == 0) {
			lstProcessFunction(&head);

		} else if (strcmp(comando, "fcfs") == 0) {
			fcfsFunction(&head, &memoria);

		} else if (strcmp(comando, "sjf") == 0) {
			sjfFunction(&head, &memoria);

		} else if (strcmp(comando, "rr") == 0) {
			rrFunction(&head, &memoria);

		} else if (strcmp(comando, "alloc") == 0 && pipe1 != NULL && archivo != NULL) {
			int id = atoi(pipe1);
			allocFunction(&head, id, archivo, &memoria);

		} else if (strcmp(comando, "free") == 0 && archivo != NULL) {
			int id = atoi(archivo);
			freeFunction(&head, id, &memoria);

		} else if (strcmp(comando, "mstatus") == 0) {
			mstatusFunction(&memoria);

		} else if (strcmp(comando, "compact") == 0) {
			compactFunction(&memoria);

		} else if (strcmp(comando, "mycat") == 0 && archivo != NULL) {
			if (pipe1 != NULL && strcmp(pipe1, ">") == 0)
				catFunction(archivo, 0);
			else
				catFunction(archivo, 1);

		} else if (strcmp(comando, "mycp") == 0 && archivo != NULL) {
			if (cpFunction(pipe1, archivo) == -1) printf("Error\n");

		} else if (strcmp(comando, "remove") == 0 && archivo != NULL) {
			if (rmFunction(archivo) == -1) printf("Error\n");

		} else if (contains(comando, commands)) {
			// Comandos bash con soporte de pipes
			char *elements[MAX_COMMANDS];
			char *element = strtok(opcionTemp, "|");
			int numElements = 0;
			while (element != NULL && numElements < MAX_COMMANDS) {
				elements[numElements] = element;
				++numElements;
				element = strtok(NULL, "|");
			}

			Command command[MAX_COMMANDS];
			for (int i = 0; i < numElements; ++i) {
				command[i].command = strtok(elements[i], " ");
				command[i].arg = strtok(NULL, " ");
				if (command[i].arg != NULL)
					command[i].arg[strcspn(command[i].arg, "\n")] = '\0';
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
					char *comTemp[] = {command[i].command, command[i].arg, NULL};
					execvp(comTemp[0], comTemp);
					exit(1);
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
	printf("ID | NAME | BURST | SIZE | STATE\n");
	while (rec != NULL) {
		printf("%d %s %d %d %s\n", rec->id, rec->name, rec->burst, rec->size, rec->state);
		rec = rec->next;
	}
}

// ============================================================
// ALGORITMOS — reciben memoria para liberar al terminar
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

		// Marcar terminated y liberar memoria
		Process_t *orig = findByID(head, cur->id);
		if (orig != NULL) {
			strcpy(orig->state, "terminated");
			freeFunction(head, orig->id, memoria);
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

		// Marcar terminated y liberar memoria
		strcpy(node->state, "terminated");
		freeFunction(head, node->id, memoria);
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
					waiting[idx] = acum - original_burst[i];
					current->burst = original_burst[i];
					break;
				}
			}

			// Marcar terminated y liberar memoria
			Process_t *orig = findByID(head, current->id);
			if (orig != NULL) {
				strcpy(orig->state, "terminated");
				freeFunction(head, orig->id, memoria);
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

	int tamano = proc->size;
	Bloque_t *bloque = NULL;

	if (strcmp(estrategia, "firstfit") == 0) {
		Bloque_t *actual = *memoria;
		while (actual != NULL) {
			if (!actual->estado && actual->tamano >= tamano) {
				bloque = actual;
				break;
			}
			actual = actual->siguiente;
		}
	} else if (strcmp(estrategia, "worstfit") == 0) {
		Bloque_t *actual = *memoria;
		while (actual != NULL) {
			if (!actual->estado && actual->tamano >= tamano) {
				if (bloque == NULL || actual->tamano > bloque->tamano)
					bloque = actual;
			}
			actual = actual->siguiente;
		}
	} else if (strcmp(estrategia, "bestfit") == 0) {
		Bloque_t *actual = *memoria;
		while (actual != NULL) {
			if (!actual->estado && actual->tamano >= tamano) {
				if (bloque == NULL || actual->tamano < bloque->tamano)
					bloque = actual;
			}
			actual = actual->siguiente;
		}
	} else {
		printf("Estrategia invalida. Usa: firstfit, worstfit, bestfit\n");
		return;
	}

	if (bloque == NULL) { printf("No hay espacio disponible\n"); return; }

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
	Bloque_t *actual = *memoria;
	while (actual != NULL) {
		if (actual->estado && actual->procesoAlojado != NULL &&
		    actual->procesoAlojado->id == id) {

			// Solo regresar a "new" si no está terminated
			if (strcmp(actual->procesoAlojado->state, "terminated") != 0) {
				strcpy(actual->procesoAlojado->state, "new");
			}

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

void compactFunction(Bloque_t **memoria) {
	Bloque_t *actual = *memoria;
	while (actual != NULL && actual->siguiente != NULL) {
		if (!actual->estado && !actual->siguiente->estado) {
			actual->tamano += actual->siguiente->tamano;
			Bloque_t *temp = actual->siguiente;
			actual->siguiente = actual->siguiente->siguiente;
			free(temp);
		} else {
			actual = actual->siguiente;
		}
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