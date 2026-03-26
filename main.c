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

int main(){
    // User can select the process mode of the uploaded image, default mode is 1
    int mode = 1;

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

    for (int j = 0; j < 10; j++){
        ImagePacket pkt;
        pkt.chunk_id = j;
        pkt.data_size = 1500;

        int target_worker;

        if (mode == 1){
            pkt.worker_id = 0;
            target_worker = j % 3;
        }else{
            target_worker = j % 3;
            pkt.worker_id = target_worker;
        }

        write(parent_to_child[target_worker][1], &pkt, sizeof(ImagePacket));
        ImagePacket result;
        if(read(parent_to_child[target_worker][0], &result, sizeof(ImagePacket)) > 0){
            printf("Parent received chunk %d (Processed by Algorithm %d) from Worker %d\n", result.chunk_id, result.worker_id, target_worker);
        }
    }

    for (int i = 0 ; i < 3; i++){
        close(parent_to_child[i][1]);
        close(child_to_parent[i][0]);
        waitpid(pids[i], NULL, 0);
    }
    return 0;
}