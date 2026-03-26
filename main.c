#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "protocol.h"

void run_worker(int read_fd, int write_fd, int id);

int main(){
    int mode = 1;
    int parent_to_child[3][2];
    int child_to_parent[3][2];
    pid_t pids[3];

    for (int i = 0; i < 3; i++){
        if (pipe(parent_to_child[i]) == -1 || pipe(child_to_parent[i]) == -1){
            perror("pipe");
            exit(1);
        }

        pids[i] = fork();
        if(pids[i] < 0){
            perror("fork");
            exit(1);
        } else if (pids[i] == 0){
            close(parent_to_child[i][1]);
            close(child_to_parent[i][0]);
            run_worker(parent_to_child[i][0], child_to_parent[i][1], i);
            exit(0);
        }
        close(parent_to_child[i][0]);
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