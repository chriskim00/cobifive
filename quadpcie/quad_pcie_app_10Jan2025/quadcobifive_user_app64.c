#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "cobi_input_data.h"

#define DEVICE_FILE_TEMPLATE "/dev/cobi_pcie_card%d"
#define READ_COUNT_COBI_CHIP_1 3
#define RAW_BYTE_CNT 166
#define ROW_SIZE 7
#define MAX_DEVICES 10

/* 
THIS APPLICATION IS FOR Quad COBIFIVE. 
Created by: RPJ
Date: Dec. 6, 2024
*/

typedef struct {
    off_t offset;
    uint64_t value;  // Change to 64-bit
} write_data_t;

uint64_t swap_bytes(uint64_t val) {;

    return ((val >> 8)  & 0x00FF00FF00FF00FF) |
           ((val << 8)  & 0xFF00FF00FF00FF00) ;
}

void perform_bulk_write(int fd, const uint64_t* data, size_t count) {
    write_data_t *bulk_write_data = (write_data_t*)malloc(count * sizeof(write_data_t));
    if (!bulk_write_data) {
        perror("Failed to allocate memory for bulk write data");
        return;
    }
    
    for (size_t i = 0; i < count; ++i) {
        bulk_write_data[i].offset = 9 * sizeof(uint64_t);  // Memory Map Address
        bulk_write_data[i].value =  swap_bytes(data[i]);   // 64-bit value
    }

    if (write(fd, bulk_write_data, count * sizeof(write_data_t)) != count * sizeof(write_data_t)) {
        perror("Failed to write bulk data to device");
    }

    free(bulk_write_data);
}

int read_empty_status(int fd, uint32_t* read_status) {
    off_t read_offset = 10 * sizeof(uint32_t); // Read FIFO empty status



    // Read data from device
    uint32_t read_data;
    if (pread(fd, &read_data, sizeof(read_data),read_offset) != sizeof(read_data)) {
        perror("Failed to read from device");
        return -1;
    }

    if (!(read_data & (1 << 5))) {
        printf("S_READY IS LOW.\n");
    }
    *read_status = read_data;
    return 0;
}

uint8_t inverse_data(uint8_t data) {
    uint8_t reversed = 0;
    for (int i = 0; i < 4; ++i) {
        reversed |= ((data >> i) & 1) << (3 - i);
    }
    return reversed;
}

int fwid_read(int fd,uint32_t* fwid_val) {
    off_t read_offset = 1 * sizeof(uint32_t); // Read FIFO empty status

    // Read data from device
    uint32_t read_data;
    if (pread(fd, &read_data, sizeof(read_data),read_offset) != sizeof(read_data)) {
        perror("Failed to read from device");
        return -1;
    }
    *fwid_val = read_data;
    return 0;

}

void perform_operations(const char* device_file) {
    int fd, i, h,k,l;
    uint32_t read_data, read_status, fwid_val; 
    write_data_t write_data;
    off_t read_offset, read_mem_check;
    struct timespec start, end;

    // Open device file
    fd = open(device_file, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return;
    }


    // COBIFIVE Initialization
    fwid_read(fd, &fwid_val);
    printf("FWID: 0x%x\n", fwid_val);
    if(fwid_val == 0xAA558823){
    printf("Writing Data to COBIFIVE\n");
        read_empty_status(fd, &read_status);
        printf("Status Register: 0x%x\n",read_status);
        printf("initialize\n");

        write_data.offset = 8 * sizeof(uint64_t);
        write_data.value = 0xFFFFFFFFFFFFFFFF;    // Initialize AXI interface control
        if (write(fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
            perror("Failed to write to device");
            close(fd);
            return;
        }
        usleep(500000); 

       // Data writing loop
        for(int loopWRRD = 0; loopWRRD < 15; ++loopWRRD) {
            printf("\nLOOP: %d\n", loopWRRD);
            int expected_reply = 0;
            for (int problem = 1; problem <= 30; ++problem) {
                printf("Writing Problem %d.\n", problem);
                switch (problem) {
                    case 1:
                        perform_bulk_write(fd, (const uint64_t*)rawData1, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 2:
                        perform_bulk_write(fd, (const uint64_t*)rawData2, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 3:
                        perform_bulk_write(fd, (const uint64_t*)rawData3, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 4:
                        perform_bulk_write(fd, (const uint64_t*)rawData4, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 5:
                        perform_bulk_write(fd, (const uint64_t*)rawData5, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        //usleep(5000);
                        break;
                    case 6:
                        perform_bulk_write(fd, (const uint64_t*)rawData6, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 7:
                        perform_bulk_write(fd, (const uint64_t*)rawData7, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 8:
                        perform_bulk_write(fd, (const uint64_t*)rawData8, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 9:
                        perform_bulk_write(fd, (const uint64_t*)rawData9, RAW_BYTE_CNT);
                    // read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        break;
                    case 10:
                        perform_bulk_write(fd, (const uint64_t*)rawData10, RAW_BYTE_CNT);
                        //read_empty_status(fd, &read_status);
                        //printf("Status Register: 0x%x\n",read_status);
                        
                        break;
                        
                    case 11:
                        perform_bulk_write(fd, (const uint64_t*)rawData11, RAW_BYTE_CNT);
                        break;
                    case 12:
                        perform_bulk_write(fd, (const uint64_t*)rawData12, RAW_BYTE_CNT);
                        break;
                    case 13:
                        perform_bulk_write(fd, (const uint64_t*)rawData13, RAW_BYTE_CNT);
                        break;
                    case 14:
                        perform_bulk_write(fd, (const uint64_t*)rawData14, RAW_BYTE_CNT);
                        break;
                    case 15:
                        perform_bulk_write(fd, (const uint64_t*)rawData15, RAW_BYTE_CNT);
                        break;
                    
                    case 16:
                        perform_bulk_write(fd, (const uint64_t*)rawData16, RAW_BYTE_CNT);
                        break;
                    case 17:
                        perform_bulk_write(fd, (const uint64_t*)rawData17, RAW_BYTE_CNT);
                        break;
                    case 18:
                        perform_bulk_write(fd, (const uint64_t*)rawData18, RAW_BYTE_CNT);
                        break;
                    case 19:
                        perform_bulk_write(fd, (const uint64_t*)rawData19, RAW_BYTE_CNT);
                        break;
                    case 20:
                        perform_bulk_write(fd, (const uint64_t*)rawData20, RAW_BYTE_CNT);
                        break;
                        
                    case 21:
                        perform_bulk_write(fd, (const uint64_t*)rawData21, RAW_BYTE_CNT);
                        break;
                    case 22:
                        perform_bulk_write(fd, (const uint64_t*)rawData22, RAW_BYTE_CNT);
                        break;
                    case 23:
                        perform_bulk_write(fd, (const uint64_t*)rawData23, RAW_BYTE_CNT);
                        break;
                    case 24:
                        perform_bulk_write(fd, (const uint64_t*)rawData24, RAW_BYTE_CNT);
                        break;
                    case 25:
                        perform_bulk_write(fd, (const uint64_t*)rawData25, RAW_BYTE_CNT);
                        break;
                    case 26:
                        perform_bulk_write(fd, (const uint64_t*)rawData26, RAW_BYTE_CNT);
                        break;
                    case 27:
                        perform_bulk_write(fd, (const uint64_t*)rawData27, RAW_BYTE_CNT);
                        break;
                    case 28:
                        perform_bulk_write(fd, (const uint64_t*)rawData28, RAW_BYTE_CNT);
                        break;
                    case 29:
                        perform_bulk_write(fd, (const uint64_t*)rawData29, RAW_BYTE_CNT);
                        break;
                    case 30:
                        perform_bulk_write(fd, (const uint64_t*)rawData30, RAW_BYTE_CNT);
                        break;
                    default:
                        printf("Invalid problem number.\n");
                        break;
                }
                //usleep(5000);   // ================   INTRODUCE DELAY PER WRITE
                read_empty_status(fd, &read_status);
                expected_reply = problem;

                if (!(read_status & (1 << 5))) {     // ================ VERY IMPORTANT TO ADD
                    printf("COBIFIVE is not ready to receive data.\n");
                    time_t start_time_wr = time(NULL); // Record the start time

                    // Wait for up to 2 seconds before breaking the loop
                    while (difftime(time(NULL), start_time_wr) < 2) {
                        read_empty_status(fd, &read_status);
                        if (read_status & (1 << 5)) {
                            printf("COBIFIVE is ready again.\n");
                            break; // Exit the timeout loop and continue with the for loop
                        }
                        //printf("waiting for 100ms.\n");
                        usleep(100000); // Check every 100ms
                    }

                    // If still not ready after 2 seconds, exit the for loop
                    if (!(read_status & (1 << 5))) {
                        printf("Timeout occurred! Exiting loop after 2 seconds.\n");
                        break;
                    }
                }
            }

        read_empty_status(fd, &read_status);
        printf("Status Register: 0x%x\n",read_status);
            usleep(5000);
            printf("Done writing data to cobi chip.\n");
    
            int j = 0; // Initialize counter
            while (1) {
                uint64_t best_spins = 0x0;
                uint64_t best_ham   = 0x0;

                if (j < expected_reply) {
                    read_empty_status(fd, &read_status);
                    printf("Status Register: 0x%x\n",read_status);
                    /*if (read_status & 1) {
                        //break;
                        printf("S_READY IS LOW.\n");
                    }*/

                    if ((read_status & 1)) {
                        time_t start_time = time(NULL);
                        while (difftime(time(NULL), start_time) < 5) {
                            read_empty_status(fd, &read_status);
                            if (!(read_status & 1)) {
                                printf("Read Fifo has Data.\n");
                                break; // Exit the timeout loop and continue with the for loop
                            }
                            usleep(100000); // Check every 100ms
                        }
                        if (!(read_status & 1)) {
                            printf("Timeout occurred! Exiting loop. No data left.\n");
                            break;
                        }
                    }
                    else{
                    //if (!(read_status & (1<<1))) {
                        j++;
                        printf("Reply: %d\n",j);
                        printf("CHIP READ DATA.\n");
                        for (i = 0; i < 3; ++i) {
                            read_offset = 4 * sizeof(uint32_t);

                         

                            // Read data from device
                            if (pread(fd, &read_data, sizeof(read_data),read_offset) != sizeof(read_data)) {
                                perror("Failed to read from device");
                                close(fd);
                                return;
                            }
                            //printf("COBIFIVE Response: 0x%x\n", read_data);
                            if(i == 0){
                                uint8_t problem_id = inverse_data(read_data);
                                printf("COBIFIVE Problem ID: 0x%x\n", problem_id);

                                uint8_t core_id = inverse_data(read_data >> 4);
                                printf("COBIFIVE Core ID: %d\n", core_id);
                                best_spins =read_data;
                            }
                            if(i == 1){
                                best_spins = ((uint64_t)read_data << 32) | (best_spins & 0xFFFFFFFF);
                                printf("COBIFIVE Best Spins: 0x%lx\n", (best_spins>>8) & 0x3FFFFFFFFFFF);
                                best_ham = read_data;
                            }
                            if(i == 2){
                                best_ham = ((uint64_t)read_data << 32) | (best_ham & 0xFFFFFFFF);
                                printf("COBIFIVE Best Ham: 0x%lx\n", (best_ham>>22) & 0x3FFF);
                                printf("COBIFIVE Chip ID: 0x%lx\n", (best_ham>>37) & 0xF);
                            }
                            
                        }
                    }

                    /*if (!(read_status & (1<<2))) {
                        j++;
                        printf("CHIP B DATA.\n");
                        for (h = 0; h < 3; ++h) {
                            read_offset = 5 * sizeof(uint32_t);

                            // Write read offset to device
                            if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
                                perror("Failed to set read offset in device");
                                close(fd);
                                return;
                            }

                            // Read data from device
                            if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
                                perror("Failed to read from device");
                                close(fd);
                                return;
                            }
                            //printf("COBIFIVE Response: 0x%x\n", read_data);
                            if(h == 0){
                                uint8_t problem_id = inverse_data(read_data);
                                printf("COBIFIVE Problem ID: 0x%x\n", problem_id);

                                uint8_t core_id = inverse_data(read_data >> 4);
                                printf("COBIFIVE Core ID: %d\n", core_id);
                                best_spins =read_data;
                            }
                            if(h == 1){
                                best_spins = ((uint64_t)read_data << 32) | (best_spins & 0xFFFFFFFF);
                                printf("COBIFIVE Best Spins: 0x%lx\n", (best_spins>>8) & 0x3FFFFFFFFFFF);
                                best_ham = read_data;
                            }
                            if(h == 2){
                                best_ham = ((uint64_t)read_data << 32) | (best_ham & 0xFFFFFFFF);
                                printf("COBIFIVE Best Ham: 0x%lx\n", (best_ham>>22) & 0x3FFF);
                                printf("COBIFIVE Chip ID: 0x%lx\n", (best_ham>>37) & 0xF);
                            }
                        }
                    }

                    if (!(read_status & (1<<3))) {
                        j++;
                        printf("CHIP C DATA.\n");
                        for (k = 0; k < 3; ++k) {
                            read_offset = 6 * sizeof(uint32_t);

                            // Write read offset to device
                            if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
                                perror("Failed to set read offset in device");
                                close(fd);
                                return;
                            }

                            // Read data from device
                            if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
                                perror("Failed to read from device");
                                close(fd);
                                return;
                            }
                            //printf("COBIFIVE Response: 0x%x\n", read_data);
                            if(k == 0){
                                uint8_t problem_id = inverse_data(read_data);
                                printf("COBIFIVE Problem ID: 0x%x\n", problem_id);

                                uint8_t core_id = inverse_data(read_data >> 4);
                                printf("COBIFIVE Core ID: %d\n", core_id);
                                best_spins =read_data;
                            }
                            if(k == 1){
                                best_spins = ((uint64_t)read_data << 32) | (best_spins & 0xFFFFFFFF);
                                printf("COBIFIVE Best Spins: 0x%lx\n", (best_spins>>8) & 0x3FFFFFFFFFFF);
                                best_ham = read_data;
                            }
                            if(k == 2){
                                best_ham = ((uint64_t)read_data << 32) | (best_ham & 0xFFFFFFFF);
                                printf("COBIFIVE Best Ham: 0x%lx\n", (best_ham>>22) & 0x3FFF);
                                printf("COBIFIVE Chip ID: 0x%lx\n", (best_ham>>37) & 0xF);
                            }
                        }
                    }
                    
                    if (!(read_status & (1<<4))) {
                        j++;
                        printf("CHIP D DATA.\n");
                        for (l = 0; l < 3; ++l) {
                            read_offset = 7 * sizeof(uint32_t);

                            // Write read offset to device
                            if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
                                perror("Failed to set read offset in device");
                                close(fd);
                                return;
                            }

                            // Read data from device
                            if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
                                perror("Failed to read from device");
                                close(fd);
                                return;
                            }
                            //printf("COBIFIVE Response: 0x%x\n", read_data);
                            if(l == 0){
                                uint8_t problem_id = inverse_data(read_data);
                                printf("COBIFIVE Problem ID: 0x%x\n", problem_id);

                                uint8_t core_id = inverse_data(read_data >> 4);
                                printf("COBIFIVE Core ID: %d\n", core_id);
                                best_spins =read_data;
                            }
                            if(l == 1){
                                best_spins = ((uint64_t)read_data << 32) | (best_spins & 0xFFFFFFFF);
                                printf("COBIFIVE Best Spins: 0x%lx\n", (best_spins>>8) & 0x3FFFFFFFFFFF);
                                best_ham = read_data;
                            }
                            if(l == 2){
                                best_ham = ((uint64_t)read_data << 32) | (best_ham & 0xFFFFFFFF);
                                printf("COBIFIVE Best Ham: 0x%lx\n", (best_ham>>22) & 0x3FFF);
                                printf("COBIFIVE Chip ID: 0x%lx\n", (best_ham>>37) & 0xF);
                            }
                        }
                    }*/
                    //printf("Reply: %d\n",j);
                    
                }
                else {
                    read_empty_status(fd, &read_status);
                    printf("Status Register: 0x%x\n",read_status);
                    printf("Done Fetching Data.\n");
                    break;
                }
            }

        }
    }
    else{
        printf("Not Quad Cobifive Card\n");
        close(fd);
    }
}
int main() {
    // Open the output_logs file for writing
    FILE *log_file = freopen("output_logs", "w", stdout);
    if (log_file == NULL) {
        perror("Failed to redirect stdout");
        return 1;
    }

    // Redirect stderr as well
    if (freopen("output_logs", "a", stderr) == NULL) {
        perror("Failed to redirect stderr");
        return 1;
    }

    char device_file[256];
    for (int i = 0; i < MAX_DEVICES; ++i) {
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, i);
        if (access(device_file, F_OK) == 0) {
            printf("\nAccessing device file: %s\n", device_file);
            //for (int k = 0; k < 5; ++k) {
            //    printf("Outer loop : %d\n", k);
                perform_operations(device_file);
            //}
        } else {
            printf("No device found at %s\n", device_file);
            break;
        }
    }
    
    fclose(log_file); // Close the file when done
    return 0;
}
