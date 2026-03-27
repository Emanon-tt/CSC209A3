/*
 * this is the main file of our project
 * these codes include the central dispatcher of the project. Responsible for reading PPM pictures and initializing pipelines
 * now it can fork subprocesse(3) and distribute pixel blocks according to the selected mode
 * mode 0 -> Blue Channel (Keep the B value unchanged and set R and G to 0)
 * mode 1 -> Invert Color (Change each RGB value from x to 255 - x)
 * mode 2 -> Psychedelic (Multiply the pixel values by 2 or 5, and utilize the automatic overflow feature of unsigned char)
 * mode 3 -> Horizontal Mirror (swapped left and right, as if looking in a mirror)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "protocol.h"

void run_worker(int read_fd, int write_fd, int id);

int main(int argc, char *argv[]){
    // Parameter check: program name + input file + 3 mode numbers
    if(argc < 5){
        fprintf(stderr, "Usage: %s <input.ppm> <m0> <m1> <m2>\n", argv[0]);
        exit(1);
    }

    char *input_filename = argv[1];

    // User can select the process mode of the uploaded image
    int selected_modes[3]; 
    for (int i = 0; i < 3; i++) {
        selected_modes[i] = atoi(argv[i + 2]);
    }
    
    // This is the parent to child pipe, which carrying out the task distribution
    int parent_to_child[3][2]; 

    // This is the child to parent pipe, which recovering the execution results
    int child_to_parent[3][2];
    pid_t pids[3];

    for (int i = 0; i < 3; i++){
        if (pipe(parent_to_child[i]) == -1 || pipe(child_to_parent[i]) == -1){
            perror("pipe"); // Error checking
            exit(1);
        }

        pids[i] = fork();
        if(pids[i] < 0){
            perror("fork"); // Error checking
            exit(1);
        } else if (pids[i] == 0){
            // in child process, immediately shot down the unnecessary pipeline endpoinrs to prevent blockage
            
            // Close the "write" of the parent_to_child pipe, the child process will only read data from this pipe
            close(parent_to_child[i][1]);

            // Close the "read" of the child_to_parent pipe, the child process will only write data to this pipe
            close(child_to_parent[i][0]);
            run_worker(parent_to_child[i][0], child_to_parent[i][1], i);
            exit(0);
        }
        // parent side
        // Close the "read" of the parent_to_child pipe, the parent process will only write data to this pipe
        close(parent_to_child[i][0]);

        // Close the "write" of the child_to_parent pipe, the parent process will only read data from this pipe
        close(child_to_parent[i][1]);
    }
    
    //prepare output files 
    FILE *out_files[3];
    char out_name[32];
    for (int i = 0; i < 3; i++){
        sprintf(out_name,"output_mode%d.ppm", selected_modes[i]);
        out_files[i] = fopen(out_name, "wb");
        if (!out_files[i]){perror("fopen"); exit(1);}
        // TODO：write_ppm_header（out_files[i]，w，h）；
        fprintf(out_files[i], "P6\n%d %d\n255\n", 100, 10); //this is a mock header, need to modify later
    }
    
    //distribution and recycling cycle(still using mock data, need to modify later
    for(int j = 0; j < 10; j++){
        ImagePacket pkt;
        pkt.chunk_id = j;
        pkt.data_size = 1500;
        
        //load_pixel_data(&pkt);

        for(int w = 0; w < 3; w++){
            pkt.worker_id = selected_modes[w];
            if (write(parent_to_child[w][1], &pkt, sizeof(ImagePacket)) == -1) {
                perror("write");
            }
        }
        for (int w = 0; w < 3; w++){
            ImagePacket result;
            if(read(child_to_parent[w][0], &result, sizeof(ImagePacket)) > 0){
                fwrite(result.payload, 1, result.data_size, out_files[w]);
            }
        }
        printf("Progress: Chunk %d processed by all 3 filters.\n", j);
    }
    




    for (int i = 0 ; i < 3; i++){
        fclose(out_files[i]);
        close(parent_to_child[i][1]);
        close(child_to_parent[i][0]);
        waitpid(pids[i], NULL, 0);
    }

    printf("Processing complete. 3 images generated.\n");
    return 0;
}