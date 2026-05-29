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

#define COMM 14
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
	char state[15];
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

char *commands[COMM] = {"mycat", "mycp", "remove", "exitt", "mkprocess", "lsprocess",
                         "fcfs", "sjf", "rr", "alloc", "free", "mstatus", "compact", "my_kill"};

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
	Process_t *head = NULL;

	while (1) {
		printf(">");
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
		char *pipe1   = (argc_cmd > 1) ? args[1] : NULL;
		char *archivo  = (argc_cmd > 2) ? args[2] : NULL;

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

		// --------------------------------------------------------
		// my_kill <id>
		// --------------------------------------------------------
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

		} else {
			// Intentar ejecutar como comando bash (con soporte de pipes)
			char *elements[MAX_COMMANDS];
			char *element = strtok(opcionTemp, "|");
			int numElements = 0;
			while (element != NULL && numElements < MAX_COMMANDS) {
				elements[numElements] = element;
				++numElements;
				element = strtok(NULL, "|");
			}

			if (element != NULL) {
				printf("Error: maximo %d comandos en pipe\n", MAX_COMMANDS);
				continue;
			}

			int prev_fd = -1;
			int fork_error = 0;
			for (int i = 0; i < numElements && !fork_error; ++i) {
				int pipes[2];
				if (i < numElements - 1) {
					if (pipe(pipes) == -1) {
						printf("error al crear el pipe %d\n", i);
						fork_error = 1;
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
					// execvp falló: comando no encontrado
					exit(127);
				}
				if (prev_fd != -1) close(prev_fd);
				if (i < numElements - 1) {
					close(pipes[1]);
					prev_fd = pipes[0];
				}
			}

			// Esperar hijos y detectar si todos fallaron con 127
			int all_failed = 1;
			for (int i = 0; i < numElements; ++i) {
				int status;
				wait(&status);
				if (WIFEXITED(status) && WEXITSTATUS(status) != 127)
					all_failed = 0;
			}
			if (all_failed)
				printf("Invalid command: %s\n", args[0]);
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
			current->burst -= QUANTUM;
			acum += QUANTUM;
			/* reinsertar al final de la cola */
			insertFInal(&copy_head, current);
		} else {
			acum += current->burst;
			/* registrar salida y liberar memoria */
			turnaround[idx] = acum;
			list[idx] = current;
			printf("[%d] sale %s\n", acum, current->name);
			strcpy(current->state, "terminated");
			freeFunction(head, current->id, memoria);
		}
		idx++;
	}

	return 0;
}

// Implementaciones mínimas para permitir compilación y pruebas
int catFunction(char *filename, int flag) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;
    int c;
    while ((c = fgetc(f)) != EOF) putchar(c);
    fclose(f);
    return 0;
}

int cpFunction(char *fileNameS, char *fileNameD) {
    FILE *src = fopen(fileNameS, "rb");
    if (!src) return -1;
    FILE *dst = fopen(fileNameD, "wb");
    if (!dst) { fclose(src); return -1; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf,1,sizeof(buf),src)) > 0) fwrite(buf,1,n,dst);
    fclose(src); fclose(dst);
    return 0;
}

int rmFunction(char *filename) {
    if (unlink(filename) == -1) return -1;
    return 0;
}

int contains(char *command, char *list[COMM]) {
    for (int i = 0; i < COMM; ++i) {
        if (list[i] && strcmp(command, list[i]) == 0) return 1;
    }
    return 0;
}

void stats(Process_t* list[], int turnaround[], int waiting[], int n) {
    // minimal: print basic stats
    for (int i = 0; i < n; ++i) {
        printf("%s turnaround=%d waiting=%d\n", list[i]->name, turnaround[i], waiting[i]);
    }
}

void allocFunction(Process_t **head, int id, char *estrategia, Bloque_t **memoria) {
    Process_t *p = findByID(head, id);
    if (!p) { printf("Process %d not found\n", id); return; }
    strcpy(p->state, "ready");
    printf("Process %d allocated (stub)\n", id);
}

void freeFunction(Process_t **head, int id, Bloque_t **memoria) {
    Process_t *p = findByID(head, id);
    if (!p) { printf("Process %d not found\n", id); return; }
    strcpy(p->state, "new");
    printf("Process %d freed (stub)\n", id);
}

void mstatusFunction(Bloque_t **memoria) {
    printf("mstatus (stub)\n");
}

void compactFunction(Bloque_t **memoria, int memoriaTotal) {
    printf("compact (stub)\n");
}

