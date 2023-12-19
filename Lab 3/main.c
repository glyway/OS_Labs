#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

#define FRAME_SIZE 256
#define PAGE_SIZE 256
#define COUNT_FRAMES 128 // Количество фреймов
#define COUNT_PAGES 256
#define TLB_SIZE 16
#define MEMORY_SIZE FRAME_SIZE * COUNT_FRAMES // Размер физической памяти

char memory[MEMORY_SIZE]; // Физическая память

int page_table[COUNT_PAGES][2]; // Таблица страниц
int tlb_table[TLB_SIZE][2]; // TLB - Буфер ассоциативной трансляции (Логический - Физический адрес)
int claim_table[COUNT_FRAMES][2]; // Таблица занятых фреймов в физической памяти (0 - занятость, 1 - количество использований)
int tlb_size = 0; // Размер TLB
int page_table_size = 0; // Размер таблицы страниц


int get_frame_number_by_page_number_from_tlb(int page_number){ // Получение фрейма из TLB с помощью номера страницы
    for (int x = 0; x < tlb_size; x++)
        if (tlb_table[x][0] == page_number) {
            return tlb_table[x][1];
        }
    return -1;
}

int get_frame_number_by_page_number_from_page_table(int page_number){ // Получение фрейма из таблицы страниц с помощью номера страницы
    for (int x = 0; x < page_table_size; x++)
        if (page_table[x][0] == page_number) {
            return page_table[x][1];
        }
    return -1;
}

int get_free_memory_frame_number() { // Получение свободного фрейма из таблицы занятых фреймов
    for (int x = 0; x < COUNT_FRAMES; x++)
        if (!claim_table[x][0]) {
            return x;
        }
    return -1;
}

int find_least_used_memory_frame_number() { // Получение наименее используемого номера фрейма из таблицы занятых фреймов
    int use_frequency = claim_table[0][1];
    int least_used_memory_frame_number_id = 0;
    for (int x = 1; x < COUNT_FRAMES; x++)
        if (claim_table[x][1] <= use_frequency) {
            use_frequency = claim_table[x][1];
            least_used_memory_frame_number_id = x;
        }
    return least_used_memory_frame_number_id;
}

void delete_page_table_pages_by_frame_number(int frame_number) { // Удаление страницы в таблице страниц по фрейму
    for (int x = 0; x < page_table_size; x++) // ??
        if (page_table[x][1] == frame_number) {
            page_table[x][0] = -1;
            memcpy(page_table[x], page_table[page_table_size - 1], 2);
            page_table_size--;
        }
}

void delete_tlb_table_tlbs_by_frame_number(int frame_number) { // Удаление содержимого в строке TLB по фрейму
    for (int x = 0; x < tlb_size; x++)
        if (tlb_table[x][1] == frame_number) {
            tlb_table[x][0] = -1;
            memcpy(tlb_table[x], tlb_table[tlb_size - 1], 2);
            tlb_size--;
        }
}

void page_table_add(int page_number, int frame_number, int index) { // Добавление страницы в таблицу страниц
    if (index == -1){
        page_table[page_table_size][0] = page_number;
        page_table[page_table_size][1] = frame_number;
        page_table_size++;
    }
    else {
        for (int i = page_table_size - 1; i >= 1; i--){
            page_table[i - 1][0] = page_table[i][0];
            page_table[i - 1][1] = page_table[i][1];
        }
        page_table[page_table_size - 1][0] = page_number;
        page_table[page_table_size - 1][1] = frame_number;
    }
}

void tlb_add(int page_number, int frame_number, int index) { // Добавление страницы в TLB
    if (index == -1){
        tlb_table[tlb_size][0] = page_number;
        tlb_table[tlb_size][1] = frame_number;
        tlb_size++;
    }
    else {
        for (int i = tlb_size - 1; i >= 1; i--){
            tlb_table[i - 1][0] = tlb_table[i][0];
            tlb_table[i - 1][1] = tlb_table[i][1];
        }
        tlb_table[tlb_size - 1][0] = page_number;
        tlb_table[tlb_size - 1][1] = frame_number;
    }
}

int main(int argc, char* argv[]) {
    FILE* file = fopen("backing_store.bin", "rb");

    memset(page_table, -1, sizeof(int) * COUNT_PAGES * 2);
    memset(tlb_table, -1, sizeof(int) * TLB_SIZE * 2);
    memset(claim_table, 0, sizeof(int) * COUNT_FRAMES * 2);

    FILE* addreses = fopen("addresses.txt", "r");
    FILE* result = fopen("out.txt", "w");

    int tlb_hits = 0;
    int page_number_faults = 0;
    int total = 0;

    int virtual_address;
    while (fscanf(addreses, "%d", &virtual_address) == 1) {
        int page_number = virtual_address >> 8;
        int offset = virtual_address & 0x00FF;
        long frame_number = -1;

        frame_number = get_frame_number_by_page_number_from_tlb(page_number);
        if (frame_number > 0)
            tlb_hits++; // Найдена запись в TLB

        if (frame_number < 0) { // Не найдена запись в TLB
            frame_number = get_frame_number_by_page_number_from_page_table(page_number);

            if (frame_number < 0) { // Не найдена страница в таблице страниц, создаём
                int free_memory_frame_number = -1;

                free_memory_frame_number = get_free_memory_frame_number();
                if (free_memory_frame_number >= 0)
                    claim_table[free_memory_frame_number][0] = 1;

                if (free_memory_frame_number < 0) { // Не найдено свободного фрейма
                    free_memory_frame_number = find_least_used_memory_frame_number();

                    delete_page_table_pages_by_frame_number(free_memory_frame_number);
                    delete_tlb_table_tlbs_by_frame_number(free_memory_frame_number);
                }

                fseek(file, page_number * PAGE_SIZE, SEEK_SET); 
                char backing_store_page[256];
                fread(backing_store_page, 1, 256, file);
                memcpy(memory + free_memory_frame_number * FRAME_SIZE, backing_store_page, PAGE_SIZE);

                frame_number = free_memory_frame_number;

                if (page_table_size >= COUNT_PAGES){
                    page_table_add(page_number, free_memory_frame_number, 0);
                }
                else {
                    page_table_add(page_number, free_memory_frame_number, -1);
                }

                if (tlb_size >= TLB_SIZE){
                    tlb_add(page_number, free_memory_frame_number, 0);
                }
                else {
                    tlb_add(page_number, free_memory_frame_number, -1);
                }

                claim_table[free_memory_frame_number][1]++;
                page_number_faults++;
            }
            else {
                if (tlb_size >= TLB_SIZE){
                    tlb_add(page_number, frame_number, 0);
                }
                else {
                    tlb_add(page_number, frame_number, -1);
                }
            }
        }

        int value = memory[frame_number * FRAME_SIZE + offset];
        int phys_addr = frame_number * FRAME_SIZE + offset;

        fprintf(result, "Virtual address: %d Physical address: %d Value: %d\n", virtual_address, phys_addr, value);
        total++;
    }

    float tlb_hit_rate = tlb_hits / (float)total;
    float page_number_fault_rate = page_number_faults / (float)total;
    printf("Частота попаданий в TLB: %f\nЧастота ошибок страниц: %f\n", tlb_hit_rate, page_number_fault_rate);

    fclose(addreses);
    fclose(result);
    fclose(file);

    return 0;
}