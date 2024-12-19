#include <iostream>
#include <cstring>
#include <cstdint>  // For uint8_t
#include <random>   // For random number generation
#include <unistd.h> // Include for usleep
#include "ftd3xx.h" // Ensure this header file is available

#define CLOCK_200 200000000 // Adjust based on actual value

// Define the size of rawData
const size_t RAW_DATA_SIZE = 1352; // Adjust size as needed
uint8_t rawData[RAW_DATA_SIZE];

// Global FT variables
FT_STATUS ftStatus = FT_OK;
FT_HANDLE ftHandle;

// Function to fill rawData with random numbers
void generateRandomData() {
    std::random_device rd;                                // Obtain a random number from hardware
    std::mt19937 eng(rd());                               // Seed the generator
    std::uniform_int_distribution<uint8_t> distr(0, 255); // Define the range

    for (size_t i = 0; i < RAW_DATA_SIZE; ++i) {
        rawData[i] = distr(eng); // Generate random number
    }
}

// Memory cleanup function
void delBufPtr(uint8_t *&bufPtr) {
    delete[] bufPtr; // Use delete[] for arrays
    bufPtr = nullptr; // Set to nullptr after deletion
}

// Data Processing Function
bool DataProc() {
    bool bResult = true;

    ULONG data_len = sizeof(rawData);
    uint8_t *buf = new uint8_t[data_len];
    if (!buf) {
        std::cerr << "Memory allocation failed." << std::endl;
        return false; // Handle allocation failure
    }

    memset(buf, 0, data_len);
    std::cout << "Start of Data Processing\n" << std::endl;

    // Drive the GPIO LOW for data transfer
    ftStatus = FT_WriteGPIO(ftHandle, 0x3, 0);
    if (FT_FAILED(ftStatus)) {
        std::cerr << "\tFT_WriteGPIO failed" << std::endl;
        bResult = false;
    } else {
        ULONG ulBytesWritten = 0;
        std::memcpy(buf, rawData, data_len);

        ftStatus = FT_WritePipe(ftHandle, 0x02, buf, data_len, &ulBytesWritten, 1000);
        if (FT_FAILED(ftStatus)) {
            std::cerr << "FT_WritePipe failed with status: " << ftStatus << std::endl;
            bResult = false;
        }  
        if (ulBytesWritten != data_len) {
            std::cerr << "Bytes written mismatch: expected " << data_len << ", wrote " << ulBytesWritten << std::endl;
            bResult = false;
        }
    }

    delBufPtr(buf); // Clean up the buffer
    return bResult;
}

// Data Reception Function
bool DataRx() {
    bool bResult = true;
    std::cout << "Start Reading Data\n" << std::endl;

    ULONG bytesRead = 0;
    unsigned char readBuf[RAW_DATA_SIZE] = {0};

    FT_FlushPipe(ftHandle, 0x82); // Flush read pipe
    ftStatus = FT_ReadPipe(ftHandle, 0x82, readBuf, sizeof(readBuf), &bytesRead, 1000);
    if (FT_FAILED(ftStatus)) {
        std::cerr << "\tFT_ReadPipe failed" << std::endl;
        return false;
    }

    if (bytesRead != sizeof(readBuf)) {
        std::cerr << "Read bytes mismatch: expected " << sizeof(readBuf) << ", but read " << bytesRead << std::endl;
        bResult = false;
    }

    // Check if received data matches rawData
    if (memcmp(readBuf, rawData, RAW_DATA_SIZE) == 0) {
        std::cout << "Data received matches the sent data." << std::endl;
    } else {
        std::cout << "Data received does not match." << std::endl;
    }

    std::cout << "Successful Process" << std::endl;
    return bResult;
}

// Main function
int main() {
    // Open the FT device
    ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus)) {
        std::cerr << "Failed to create FT handle." << std::endl;
        return 1; // Use 1 for error exit
    }

    // Enable GPIO
    ftStatus = FT_EnableGPIO(ftHandle, 0x3, 0x3);
    if (FT_FAILED(ftStatus)) {
        std::cerr << "\tFT_EnableGPIO failed" << std::endl;
        FT_Close(ftHandle);
        return 1; // Use 1 for error exit
    }

    generateRandomData(); // Generate random data

    if (DataProc()) {
        std::cout << "Data processing completed successfully." << std::endl;
        FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x02);
    } else {
        std::cout << "Data processing failed." << std::endl;
    }

    if (DataRx()) {
        std::cout << "Data reception completed successfully." << std::endl;
    } else {
        std::cout << "Data reception failed." << std::endl;
    }

    FT_Close(ftHandle);
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x82);

    return 0;
}
