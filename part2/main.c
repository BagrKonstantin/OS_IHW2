#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>

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

typedef struct {
  sem_t p;
  sem_t dd;
  sem_t d;
} shared_sems_t;

sem_t *sem_patients;
sem_t *sem_duty_doctors;
sem_t *sem_doctors;

void cleanup() {
    sem_destroy(sem_patients);
    sem_destroy(sem_duty_doctors);
    sem_destroy(sem_doctors);
    shm_unlink("/shared_data");
    shm_unlink("/shared_sems");
}

void sigint_handler(int sig) {
    cleanup();
    exit(0);
}



int main() {
    // Создаем разделяемую память и инициализируем ее
    int fd = shm_open("/shared_data", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(shared_data_t));
    shared_data_t *shared_data = (shared_data_t*) mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shared_data->patient_index = 0;


    int fdd = shm_open("/shared_sems", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(shared_sems_t));
    shared_sems_t *shared_sems = (shared_sems_t*) mmap(NULL, sizeof(shared_sems_t), PROT_READ | PROT_WRITE, MAP_SHARED, fdd, 0);
    sem_patients = &shared_sems->p;
    sem_duty_doctors = &shared_sems->dd;
    sem_doctors = &shared_sems->d;

    signal(SIGINT, sigint_handler);

    // Создаем  неименованные семафоры для синхронизации доступа к разделяемой памяти
    sem_init(sem_patients, 1, 0);
    sem_init(sem_duty_doctors, 1, NUM_DUTY_DOCTOR);
    sem_init(sem_doctors, 1, NUM_DOCTORS);

    // Создаем процессы для дежурных врачей
    for (int i = 0; i < NUM_DUTY_DOCTOR; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Код для процесса дежурного врача
            while (1) {
                // Ожидаем нового пациента
                sem_wait(sem_patients);

                // Находим следующего пациента в очереди и отправляем его к врачу
                int patient_index = shared_data->patient_index++;
                patient_t *patient = &shared_data->patients[patient_index];
                printf("Дежурный врач %d принял пациента %d и отправил его к врачу %d\n", i, patient->id, patient->doctor_id);
                sleep(1);

                // Освобождаем семафор для врачей
                sem_post(sem_doctors);
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
                sem_wait(sem_doctors);

                // Находим следующего пациента в очереди и лечим его
                int patient_index = shared_data->patient_index - 1;
                patient_t *patient = &shared_data->patients[patient_index];
                printf("Врач %d начал лечение пациента %d\n", i, patient->id);
                sleep(2);
                printf("Врач %d закончил лечение пациента %d\n", i, patient->id);
                // Освобождаем семафор для дежурных врачей
                sem_post(sem_duty_doctors);
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
            sem_wait(sem_duty_doctors);
            printf("Пациент %d подошел к дежурному врачу и рассказал о своих проблемах\n", patient.id);

            sleep(rand() % 5);

            // Записываем информацию о пациенте в разделяемую память и увеличиваем счетчик пациентов
            shared_data->patients[shared_data->patient_index++] = patient;
            printf("Пациент %d записался на прием к врачу %d\n", patient.id, patient.doctor_id);

            // Освобождаем семафор для пациентов
            sem_post(sem_patients);

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
