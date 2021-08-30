/**
 * -----------------------------------------------------------------------------------
 * Virtual Memory Manager by Pao Yu
 * -----------------------------------------------------------------------------------
 * Virtual Memory Manager that can translate logical to physical addresses. Handles
 * page faults using the Demand Paging Algorithm.
 * ----------------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>

#define UNMAPPED                     -1
#define FRAME_SIZE                   256
#define FRAME_NUMBER_OFFSET_BITS     8
#define PAGE_NUMBER_OFFSET_BITS      8
#define PAGE_OFFSET_MASK             255
#define PAGE_TABLE_SIZE              256
#define PAGE_SIZE                    256
#define PHYSICAL_MEMORY_SIZE         PAGE_TABLE_SIZE * PAGE_SIZE

 /** STRUCT: VirtualAddress
* A data type that represents a virtual/logical address
* with an integer address, a page number, and a page offset.
* */
struct VirtualAddress {
    int address;
    int page_number;
    int page_offset;
} typedef VirtualAddress;

/** STRUCT: Physical Address
 * A data type that represents a physical address
 * with an integer address, a frame number,
 * a frame offset, and the value in the address.
 * */
struct PhysicalAddress {
    int address;
    int frame_number;
    int frame_offset;
    signed char value;
} typedef PhysicalAddress;

/** STRUCT: Virtual Memory
 * A data type that represents a list of
 * logical addresses and how many there are.
 * */
struct VirtualMemory {
    int address_count;
    VirtualAddress* addresses;
} typedef VirtualMemory;

/** STRUCT: Physical Address
 * A data type that represents a list of physical
 * addresses, how many there are, and a pointer to
 * the beginning of the actual physical address space.
 * Also includes an index tracker to track the next
 * available frame within the physical memory space.
 * */
struct PhysicalMemory {
    int address_count;
    signed char* space;
    unsigned char next_available_frame_index;
    PhysicalAddress* addresses;
} typedef PhysicalMemory;

/**
 * STRUCT: PageTable
 * A data type that represents a page table with
 * mappings between indexes (page numbers) and the
 * frame number associated with that index. Also
 * tracks fault counts during mapping.
 * */
struct PageTable {
    int* map;
    int fault_count;
} typedef PageTable;

void map_addresses(VirtualMemory* virtual_memory, PhysicalMemory* physical_memory, PageTable* page_table, FILE* backing_store, FILE* output_file);
VirtualMemory* create_virtual_memory(FILE* file_input);
PhysicalMemory* create_physical_memory();
PageTable* create_page_table();

/**
 * ENTRY POINT: The main entry point of the program
 * */
int main(int argc, char* argv[]) {

    /// Show error message if the count of required arguments is incorrect.
    if (argc != 2) {
        printf("Usage: %s addresses.txt\n", argv[0]);
        exit(0);
    }

    /// Open the input file, the output file, and the backing store.
    FILE* file_input = fopen(argv[1], "r");
    FILE* file_output = fopen("output.txt", "w");
    FILE* file_backing_store = fopen("BACKING_STORE.bin", "rb");

    /// Generate error checking message depending on the file open state.
    if (file_input == NULL) {
        printf("Error: unable to open %s\n", argv[1]);
        exit(-1);
    }

    if (file_output == NULL) {
        printf("Error: unable to open output.txt\n");
        exit(-2);
    }

    if (file_backing_store == NULL) {
        printf("Error: unable to open the backing store 'BACKING_STORE.bin'\n");
        exit(-3);
    }

    /// Create a virtual memory struct using the input addresses.
    VirtualMemory* virtual_memory = create_virtual_memory(file_input);

    /// Create an empty physical memory space with no pages in it.
    PhysicalMemory* physical_memory = create_physical_memory();

    /// Create a page table with unmapped frames.
    PageTable* page_table = create_page_table();

    /// Implement the demand paging algorithm, translating each virtual address
    /// to a physical address and then bring in missing pages from the backing store
    /// then output the result to the file "output.txt"
    map_addresses(virtual_memory, physical_memory, page_table, file_backing_store, file_output);

    /// Close all the file descriptors
    fclose(file_input);
    fclose(file_output);
    fclose(file_backing_store);

    exit(0);
}

/**
 * FUNCTION map_addresses()
 * Translates virtual addresses from a VirtualAddress struct into
 * physical addresses using demand paging. Outputs the result to a text file.
 * */
void map_addresses(VirtualMemory* virtual_memory, PhysicalMemory* physical_memory, PageTable* page_table, FILE* backing_store, FILE* output_file) {

    /// Create a buffer for reading pages from the backing store
    signed char* page_read_buffer = (signed char*)malloc(sizeof(signed char) * PAGE_SIZE);

    /// Resize the physical address list according to the number of available virtual addresses
    physical_memory->addresses = realloc(physical_memory->addresses, sizeof(PhysicalAddress) * virtual_memory->address_count);

    /// For each virtual address
    for (int i = 0; i < virtual_memory->address_count; i++) {

        /// Track the virtual address's attributes
        int va_page_address = virtual_memory->addresses[i].address;
        int va_page_offset = virtual_memory->addresses[i].page_offset;
        int va_page_number = virtual_memory->addresses[i].page_number;

        /// Translate the Virtual Address into a Physical Address
        /// The frame number is obtained from the page table[page number]
        /// The frame offset is obtained form the page offset
        int pa_frame_number = page_table->map[va_page_number];
        int pa_frame_offset = va_page_offset;

        /// If there is no frame number in the page table index selected
        if (pa_frame_number == UNMAPPED) {
            /// Add one to the fault counter
            page_table->fault_count++;

            /// Get a free frame number from the physical memory
            pa_frame_number = physical_memory->next_available_frame_index;

            /// Set the next free frame for the next frame number request
            physical_memory->next_available_frame_index++;

            /// Go to the backing store's location that corresponds to the missing unmapped page number
            fseek(backing_store, va_page_number * PAGE_SIZE, SEEK_SET);

            /// Copy the contents of the backing store to the read buffer
            fread(page_read_buffer, sizeof(signed char), PAGE_SIZE, backing_store);
            for (int i = 0; i < PAGE_SIZE; i++) {
                physical_memory->space[(pa_frame_number * FRAME_SIZE) + i] = page_read_buffer[i];
            }

            /// Add the mapped frame number with actual page contents into the
            /// page table map so that it can be accessed later on.
            page_table->map[va_page_number] = pa_frame_number;
        }

        /// For convenience, all of the information we retrieve is stored in a Physical Address struct
        /// and stored in a PhysicalMemory struct for later access.

        /// Create a new physical address
        PhysicalAddress* physical_address = (PhysicalAddress*)malloc(sizeof(PhysicalAddress));

        /// Add the frame number and the offset we got earlier
        physical_address->frame_number = pa_frame_number;
        physical_address->frame_offset = pa_frame_offset;

        /// Generate the address by combining the frame number and the frame offset using bit shift and bitwise OR
        physical_address->address = (physical_address->frame_number << FRAME_NUMBER_OFFSET_BITS) | physical_address->frame_offset;

        /// Obtain the value of associated address from the space using the frame number, offset, and page size,
        /// since it is guaranteed to have a page there now from demanding it earlier if it is missing
        physical_address->value = physical_memory->space[pa_frame_offset + (pa_frame_number * PAGE_SIZE)];
        physical_memory->addresses[pa_frame_number] = (*physical_address);

        /// Increase the address count within the physical memory (for debug and error checking).
        physical_memory->address_count++;

        /// Output each translation, mapping, and associated value into the output file
        fprintf(output_file, "Virtual address: %d Physical address: %d Value: %d\n", va_page_address, physical_memory->addresses[pa_frame_number].address, physical_memory->addresses[pa_frame_number].value);
    }

    /// Output the final statistics into the output file
    fprintf(output_file, "Page Faults = %d\n", page_table->fault_count);
    fprintf(output_file, "Page Fault Rate = %.3f\n", (float)page_table->fault_count / (float)virtual_memory->address_count);
    printf("Successfully generated output file 'output.txt'\n");
}

/**
 * FUNCTION create_virtual_memory()
 * Creates a virtual memory space, with a list of virtual addresses
 * and their extracted page number and page offsets from an input file
 * containing a variable amount of virtual addresses.
*/
VirtualMemory* create_virtual_memory(FILE* file_input) {

    /// Create a new virtual memory space
    VirtualMemory* new_virtual_memory = (VirtualMemory*)malloc(sizeof(VirtualMemory));
    new_virtual_memory->address_count = 0;

    /// Set buffers for reading a line from the input file
    char  buffer_char;
    char* buffer_line_chars = malloc(sizeof(char));
    int   buffer_line_index = 0;

    /// Scan each character until the end of the file.
    while ((buffer_char = getc(file_input)) != EOF) {

        if (buffer_char != '\n') {
            /// If within a line, store the characters in the line buffer
            buffer_line_chars = realloc(buffer_line_chars, sizeof(char) * (buffer_line_index + 1));
            buffer_line_chars[buffer_line_index] = buffer_char;
            buffer_line_index++;


        } else if (buffer_char == '\n') {
            /// If at the end of the line, create a new virtual address from the contents
            VirtualAddress* new_address = (VirtualAddress*)malloc(sizeof(VirtualAddress));
            /// Convert the characters to integers
            new_address->address = atoi(buffer_line_chars);
            /// Get the page number by shifting the bits a set size
            new_address->page_number = new_address->address >> PAGE_NUMBER_OFFSET_BITS;
            /// Get the page offset by masking the leftmost bits a set size
            new_address->page_offset = new_address->address & PAGE_OFFSET_MASK;
            /// Resize the address list to accomodate the new address
            new_virtual_memory->addresses = realloc(new_virtual_memory->addresses, sizeof(VirtualAddress) * (new_virtual_memory->address_count + 1));
            /// Add the newly created address to the virtual memory's address list
            new_virtual_memory->addresses[new_virtual_memory->address_count] = (*new_address);
            /// Increase the count of addresses.
            new_virtual_memory->address_count++;

            /// Reset the line buffer for the next line.
            for (int i = 0; i < (buffer_line_index + 1); i++) {
                buffer_line_chars[i] = 0;
            }
            buffer_line_index = 0;
        }
    }
    /// Return the pointer to the new virtual memory.
    return new_virtual_memory;
}

/**
 * FUNCTION: create_physical_memory()
 * Creates a Physical memory space using a predetermined size.
 * This memory space's frames is all free (There are no pages in it yet.)
 * */
PhysicalMemory* create_physical_memory() {
    PhysicalMemory* new_physical_memory = (PhysicalMemory*)malloc(sizeof(PhysicalMemory));
    new_physical_memory->addresses = (PhysicalAddress*)malloc(sizeof(PhysicalAddress));
    new_physical_memory->space = (signed char*)malloc(sizeof(signed char) * PHYSICAL_MEMORY_SIZE);
    new_physical_memory->next_available_frame_index = 0;
    new_physical_memory->address_count = 0;
    return new_physical_memory;
}

/**
 * FUNCTION: create_page_table()
 * Creates and initializes an empty page table with no page number
 * to frame number mappings. All the values are set to -1 to indicate
 * that there is no mapping.
*/
PageTable* create_page_table() {
    PageTable* new_page_table = (PageTable*)malloc(sizeof(PageTable));
    new_page_table->map = malloc(sizeof(int) * PAGE_TABLE_SIZE);
    new_page_table->fault_count = 0;
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        new_page_table->map[i] = UNMAPPED; /* UNMAPPED == -1 */
    }
    return new_page_table;
}