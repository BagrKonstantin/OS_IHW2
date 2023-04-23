#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define NUM_DUTY_DOCTOR 2
#define NUM_DOCTORS 3
#define NUM_PATIENTS 10

// Структура для хранения информации о пациенте
typedef struct {
  int id;
  int doctor_id; // ID врача, к которому отправлен пациент
} patient_t;

// Структура для разделяемой памяти
typedef struct {
  int patient_index; // Индекс следующего пациента в очереди
  patient_t patients[NUM_PATIENTS]; // Массив пациентов
} shared_data_t;

int sem_patients;
int sem_duty_doctors;
int sem_doctors;

int mem;

void cleanup() {
    shmctl(mem, IPC_RMID, NULL);
    semctl(sem_patients, 0, IPC_RMID);
    semctl(sem_duty_doctors, 0, IPC_RMID);
    semctl(sem_doctors, 0, IPC_RMID);
}

void sigint_handler(int sig) {
    cleanup();
    exit(0);
}



int main() {
    // Создаем разделяемую память и инициализируем ее



    key_t semkey = ftok("semfile", 1);
    sem_patients = semget(semkey, 2, 0666 | IPC_CREAT);
    semctl(sem_patients, 0, SETVAL, num_bees);

    sem_duty_doctors = semget(semkey, 2, 0666 | IPC_CREAT);
    semctl(sem_duty_doctors, 0, SETVAL, num_bees);

    sem_doctors = semget(semkey, 2, 0666 | IPC_CREAT);
    semctl(sem_doctors, 0, SETVAL, num_bees);

    key_t shmkey = ftok("shmfile", 1);
    mem = shmget(shmkey, sizeof(shared_data_t), 0644 | IPC_CREAT);
    shared_data_t* shared_data = (shared_data_t *) shmat(mem, NULL, 0);


    signal(SIGINT, sigint_handler);



    // Создаем процессы для дежурных врачей
    for (int i = 0; i < NUM_DUTY_DOCTOR; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Код для процесса дежурного врача
            while (1) {
                // Ожидаем нового пациента
                semop(sem_patients, &shared_data, 1);

                // Находим следующего пациента в очереди и отправляем его к врачу
                int patient_index = shared_data->patient_index++;
                patient_t *patient = &shared_data->patients[patient_index];
                printf("Дежурный врач %d принял пациента %d и отправил его к врачу %d\n", i, patient->id, patient->doctor_id);
                sleep(1);

                // Освобождаем семафор для врачей
                semop(sem_doctors, &shared_data, 1);
            }
            exit(0);
        }
    }

    // Создаем процессы для врачей
    for (int i = 0; i < NUM_DOCTORS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Код для процесса врача
            while (1) {
                // Ожидаем нового пациента
                semop(sem_doctors, &shared_data, 1);


                // Находим следующего пациента в очереди и лечим его
                int patient_index = shared_data->patient_index - 1;
                patient_t *patient = &shared_data->patients[patient_index];
                printf("Врач %d начал лечение пациента %d\n", i, patient->id);
                sleep(2);
                printf("Врач %d закончил лечение пациента %d\n", i, patient->id);
                // Освобождаем семафор для дежурных врачей
                semop(sem_duty_doctors, &shared_data, 1);

            }
            exit(0);
        }
    }

// Создаем процессы для пациентов
    for (int i = 0; i < NUM_PATIENTS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Код для процесса пациента
            patient_t patient;
            patient.id = i;
            patient.doctor_id = rand() % NUM_DOCTORS;
            // Ожидаем доступа к дежурному врачу
            semop(sem_duty_doctors, &shared_data, 1);
            printf("Пациент %d подошел к дежурному врачу и рассказал о своих проблемах\n", patient.id);

            sleep(rand() % 5);

            // Записываем информацию о пациенте в разделяемую память и увеличиваем счетчик пациентов
            shared_data->patients[shared_data->patient_index++] = patient;
            printf("Пациент %d записался на прием к врачу %d\n", patient.id, patient.doctor_id);

            // Освобождаем семафор для пациентов
            semop(sem_patients, &shared_data, 1);

            exit(0);
        }
    }

// Ждем завершения всех процессов
    for (int i = 0; i < NUM_DUTY_DOCTOR + NUM_DOCTORS + NUM_PATIENTS; i++) {
        wait(NULL);
    }

// Удаляем семафоры и разделяемую память
    cleanup();

    return 0;
}
