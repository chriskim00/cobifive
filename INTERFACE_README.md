# Virtual Driver Documentation

## Example Interface Connection
To connect to the virtual driver, it is accessed through the use of a file path. Linux write() and read() functions are use to send problems to the COBI chip queue module.
        
        //create a string using 
        char device_file[256] = snprintf(device_file, sizeof(device_file), "/dev/cobi_chip_vdriver64");

        //check to see if the module file exisits
        if (access(device_file, F_OK) == 0) {
            printf("Accessing device file: %s\n", device_file);
        } else {
            printf("No device found at %s\n", device_file);
        }
        
        //Open the file and save the descriptor 
        int fd = open(device_file, O_RDWR);

        //check if you failed to open the device
        if (fd < 0) {
            printf("Failed to open device file '%s': %s (errno: %d)\n", device_file, strerror(errno), errno);
        }

        //Write problems to the device
        uint64_t *data = (uint64_t *)malloc(problem_count * 166 * sizeof(uint64_t));
        uint64_t user_id = write(fd, data, sizeof(data));

        //Read solutions
        uint64_t* read_data = (uint64_t *)malloc((2 + 2 * problem_count) * sizeof(uint64_t));
        read_data[0] = user_id;
        read_data[1] = problem_count;
        read(fd, read_data, sizeof(read_data)

        //Close the device
        close(fd);

## write() inputs and outputs
write(struct file *, const char *, size_t)
* `struct file *` - A pointer to the device driver file structure. ("fd" in the example code)
* `const char *` - A uint64_t pointer ( uint64_t * ) array to the buffer containing the data to be written. The array indices count must be a multiple of 166. 
* `size_t` - The number of bytes to write

returns = Sucess: (uint64_t) user_id that will be used to recover solutions on sucess. Failure: returns negative integer associated with failure type.

## read() inputs and outputs

read(struct file *, const char *, size_t)
* `struct file *` - A pointer to the device driver file structure. ("fd" in the example code)
* `const char *` - A uint64_t pointer ( uint64_t * ) array to the buffer containing the data to be written. The array indices count must be equal to: 2 + 2 * problem_count. Must be preintialized. 
* `size_t` - The number of bytes to be read

returns = Sucess: (uint64_t) 0 on sucess. Failure: returns negative integer associated with failure type.