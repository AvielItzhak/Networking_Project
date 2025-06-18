#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "udp_file_transfer.h"

#define PORT 55555
#define MAX_BUFFER_SIZE 1024
#define Packet_Max_SIZE 512
#define UPLOAD_Folder_NAME "./UploadedFiles"

/* OP_CODE */
#define UPLOAD      1 
#define DOWNLAOD    2
#define DATA        3
#define ACK         4 
#define ERROR       5
#define DELETE      6

typedef struct {// Request INFO
    int16_t CurOP_ID;
    char CurOP_FilePATH[256];
    size_t FilePATH_len;
    char CurOP_Detail[256];
} CurrRequestINFO;



int main() {
    
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    

    // Main client Request and Data transfer Handler loop //
    while (1) 
    {
         char ErrorHandler[256] = {0}; // Error string initialition
         char ACK_Response[4] = {0}; 

        printf("TFTP Server listening on port %d...\n", PORT);

        // Recive Request from client
        int Req_msg_rec = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,
                                      (struct sockaddr *)&client_addr, &addr_len);
        if (Req_msg_rec <= 0) {
            perror("Error receiving message\n"); // Print Error detail in server terminal

            // Save Error detail in ErrorHandler and send it to Client
            sprintf(ErrorHandler,"Error receiving message: %s", strerror(errno));
            sendto(sockfd, ErrorHandler, 256, 0,(const struct sockaddr *)&client_addr, addr_len);

            printf("\nResponse sent to client\nListinig to further Request.....\n\n\n");
            continue; // Continue listening
        }
         if (Req_msg_rec > 0) { // Print message recieved in bytes
            printf("Request Message (%d bytes):\n", Req_msg_rec);
            for (int i = 0; i < Req_msg_rec; i++) {
                printf("%02X ", buffer[i]);
            }
            printf("\n\n");
        }


        // Translating the information from the message
        CurrRequestINFO ClientRequest = {0}; // Creating a Request struct to handle message info 

        if (0 != Convert_REQBytes_String(buffer, &ClientRequest.FilePATH_len, &ClientRequest.CurOP_ID, ClientRequest.CurOP_FilePATH)){
            printf("Request INFO:\nOperationID - %d : FilePATH/NAME - %s\n\n",ClientRequest.CurOP_ID,ClientRequest.CurOP_FilePATH);}
        else{
            printf("Error while trying translating");
        }
        

        /* Request Handler - DELETE || UPLOAD || DOWNLOAD:
            * DELETE - Deleting the file that client Requested and print and send the result detail.
            * DOWNLOAD - 
            * UPLOAD - 
         */ 

        // DELETE Request Handler section //
        if (ClientRequest.CurOP_ID == DELETE)
        {
            // Performing DELETE operation from server
            if (RequestHandler_Delete(ClientRequest.CurOP_FilePATH,ClientRequest.CurOP_Detail) != 0)
                {printf("somthing went wrong...");}

            // Printing result detail    
            printf("%s\n\n",ClientRequest.CurOP_Detail);
            
            // Sending detail back to client
            sendto(sockfd, ClientRequest.CurOP_Detail, strlen(ClientRequest.CurOP_Detail), 0,
                                 (const struct sockaddr *)&client_addr, addr_len);
            printf("\nResponse sent to client\nListinig to further Request.....\n\n\n");
            
        }//END of DELETE Handler section 


        // UPLOAD Request Handler section //
        if (ClientRequest.CurOP_ID == UPLOAD)
        {
            // In case Upload folder doesn't exist create it
            if (access(UPLOAD_Folder_NAME, F_OK) != 0){
                if (mkdir(UPLOAD_Folder_NAME,0755) == 0) {
                    printf("Directory '%s' created successfully.\n", UPLOAD_Folder_NAME);
                }else {
                    perror("Error creating directory\n");
                    exit(EXIT_FAILURE);
                }
            }

            // Creating a new FILE with the given name
            size_t FilePATHinServer_len = strlen(UPLOAD_Folder_NAME) + 1 + sizeof(ClientRequest.CurOP_FilePATH) + 1;
            char FilePATHinServer[MAX_BUFFER_SIZE] ;
            FILE* Fpoint; // Itreator traverse file

            snprintf(FilePATHinServer, FilePATHinServer_len, "%s/%s", UPLOAD_Folder_NAME, ClientRequest.CurOP_FilePATH);
           
            while (access(FilePATHinServer, F_OK) == 0) { 
               // Handels file of same name already in server
               strcat(FilePATHinServer, "_copy");

            }  
            Fpoint = fopen(FilePATHinServer,"ab"); // Creating a poineter file to scan the intented file
            

            // Building ACK Response and sending to client
            ACK_Build_send(sockfd, client_addr, addr_len,ACK_Response, 0);
            printf("\nResponse sent to client\nAwaiting Data packets.....\n\n");


            // Reciving packet and sending ACK
            while (1) 
            {
                // Recive Request from client
                int Req_msg_rec = recvfrom(sockfd, buffer, Packet_Max_SIZE, 0,
                                              (struct sockaddr *)&client_addr, &addr_len);

                if ( Req_msg_rec <= 0) { // ERROR
                    perror("Error receiving message\n"); // Print Error detail in server terminal
                
                    // Save Error detail in ErrorHandler and send it to Client
                    sprintf(ErrorHandler,"Error receiving message: %s", strerror(errno));
                    sendto(sockfd, ErrorHandler, 256, 0,(const struct sockaddr *)&client_addr, addr_len);
                
                    printf("\nResponse sent to client\nListinig to further Request.....\n\n\n");
                    continue; // Continue listening
                }

                if ( Req_msg_rec > 4 ) // Print message recieved in bytes
                {
                   // Printing Message
                   printf("DATA Message (%d bytes):\n", Req_msg_rec);
                   for (int i = 0; i < 4; i++) 
                       {printf("%02X ", buffer[i]);}
                   printf("\n\n");

                   // Copying DATA to File in the server
                   size_t actual_bytes_written = fwrite((buffer + 4), 1, (Req_msg_rec - 4), Fpoint);

                   printf("DEBUG: fwrite call completed. Actual bytes written: %zu\n", actual_bytes_written);
 
                }
            
                if ( Req_msg_rec == 4 || Req_msg_rec != 512 ) { // Reached END OF FILE

                    // Building ACK Response and sending to client
                    ACK_Build_send(sockfd, client_addr, addr_len,ACK_Response, (int16_t)((buffer[2] << 8) | buffer[3]));
                    printf("\r\r\nEOF: UPLOAD completed successfully\nResponse sent to client\n\n");
                    break;
                }

                
                // If reach here in the loop: (the order of the if condition is relvant)
                // Building ACK Response and sending to client
                ACK_Build_send(sockfd, client_addr, addr_len,ACK_Response, (int16_t)((buffer[2] << 8) | buffer[3]));
                printf("\nMessage Recived. Response sent to client\nAwaiting further Data packets.....\n\n");

            }
            fclose(Fpoint);  

        }//END of UPLOAD Handler section 

           
        
       
    }//END of client Request and Data transfer Handler loop


    close(sockfd);
    return 0;

}//END of main server