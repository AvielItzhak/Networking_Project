#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "udp_file_transfer.h"

#define PORT 55555
#define UPLOAD_Folder_NAME "./UploadedFiles"
#define BACKUP_Folder_NAME "./Backup"



int main() {
    
    unsigned char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Backup folder
    // In case Upload folder doesn't exist create it
    if (access(BACKUP_Folder_NAME, F_OK) != 0){
        if (mkdir(BACKUP_Folder_NAME,0755) == 0) {
            printf("Directory '%s' created successfully.\n", BACKUP_Folder_NAME);
        }else {
            perror("Error creating directory\n");
            exit(EXIT_FAILURE);
        }
    }



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
        unsigned char ACK_Response[4] = {0}; 

        printf("\n\nTFTP Server listening on port %d...\n", PORT);

        // Defining Timeout for connection and Settin it as OFF
        struct timeval tv;
        tv.tv_sec = 0; 
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

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
            * DOWNLOAD - Awaiting ACK and intiating DOWNLOAD sequence - Send packet, check Response
            * UPLOAD - Sending ACK to initiate UPLOAD sequnce - get packet, send Response 
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

            // Creating a new FILE with the given name for Backup
            size_t Backup_FilePATHinServer_len = strlen(BACKUP_Folder_NAME) + 1 + sizeof(ClientRequest.CurOP_FilePATH) + 1;
            char Backup_FilePATHinServer[MAX_BUFFER_SIZE] ;
            FILE* Backup_Fpoint; // Itreator traverse file

            snprintf(Backup_FilePATHinServer, Backup_FilePATHinServer_len, "%s/%s", BACKUP_Folder_NAME, ClientRequest.CurOP_FilePATH);

            // Handels file with same name in server upload folder and assigin diffrent name
            while (access(FilePATHinServer, F_OK) == 0) 
                {strcat(FilePATHinServer, "_copy");}

            while (access(Backup_FilePATHinServer, F_OK) == 0) 
                {strcat(Backup_FilePATHinServer, "_copy");}
            
            // Creating a poineter file to scan the intented file
            Fpoint = fopen(FilePATHinServer,"ab"); 
            Backup_Fpoint = fopen(Backup_FilePATHinServer,"ab");


            // Building ACK Response and sending to client to start transfer DATA
            u_int16_t Last_SeqNUM = 0, Cur_SeqNUM = 0; // define and last SeqNUM in vars for the transfer loop 
            ACK_Build_send(sockfd, client_addr, addr_len,ACK_Response, Cur_SeqNUM);
            printf("\nResponse sent to client\nAwaiting Data packets.....\n\n");



            // Set TIMEOUT for connection
            tv.tv_sec = TimeoutValue; 
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

            int count = 0;
            // Reciving packet and sending ACK loop
            while (1) 
            {
                int Req_msg_rec = 0;// initilaztion

                // Reciving bytes from client
                Req_msg_rec = recvfrom(sockfd, buffer, Packet_Max_SIZE, 0,
                                    (struct sockaddr *)&client_addr, &addr_len);
                
                if (Req_msg_rec < 4) {
                    // Check Timeout ERROR
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                        perror("\nTimeout occurred while waiting for data\n");
                        if (count < RetryCount) // Didn't Reach Max Retrys -> Send again ACK Response
                        {
                            printf("\nRetrasmit Last SeqNUM ACK Response\n\n");
                            count ++;
                        }
                        else // Reach Max Retrys -> EXIT
                        {
                            // In case of timeout midway --> Deleting partial file left in Upload folder
                            if (access(FilePATHinServer, F_OK) == 0) // Check if file exists
                            {
                                if (RequestHandler_Delete(FilePATHinServer, ClientRequest.CurOP_Detail) != 0) {
                                     printf("\nSomething went wrong trying deleting partial file...");
                                }
                                printf("%s\n\n", ClientRequest.CurOP_Detail);
                            } 
                            else // File not exist
                                {printf("\nPartial file %s not found.\n", FilePATHinServer);}

                            printf("\nEnding Transfer session now due to timeout\n\n");
                            break; 
                        }    
                    } 
                    else { // Another type of ERROR
                        perror("Error receiving message\n");
                        // Save Error detail in ErrorHandler and send it to Client
                        sprintf(ErrorHandler, "Error receiving message: %s", strerror(errno));
                        sendto(sockfd, ErrorHandler, 256, 0, (const struct sockaddr *)&client_addr, addr_len);
                        printf("\nResponse sent to client\nListening to further Request...\n\n\n");
                        break; 
                    }
                }                    

                if ( Req_msg_rec > 4 ) // Print message recieved in bytes
                {
                   // Printing Message
                   printf("DATA Message (%d bytes):\n", Req_msg_rec);
                   for (int i = 0; i < 4; i++) 
                       {printf("%02X ", buffer[i]);}
                   printf("\n\n");

                   // Checkin to see if given the right order packet
                   memcpy(&Cur_SeqNUM, buffer + 2, 2);
                   Cur_SeqNUM = ntohs(Cur_SeqNUM); 

                   if (Last_SeqNUM == Cur_SeqNUM - 1) // Only if the right order it will Write the DATA
                   {
                        // Copying DATA to File in the server - Removing the OP and SeqNUM bytes
                        fwrite((buffer + 4), 1, (Req_msg_rec - 4), Fpoint);
                        fwrite((buffer + 4), 1, (Req_msg_rec - 4), Backup_Fpoint);
                        Last_SeqNUM = Cur_SeqNUM;
                   }   
                }
            
                if ( Req_msg_rec == 4 || Req_msg_rec != 512 ) { // Reached END OF FILE

                    // Building ACK Response and sending to client
                    memcpy(&Cur_SeqNUM, buffer + 2, 2);
                    Cur_SeqNUM = ntohs(Cur_SeqNUM); 

                    ACK_Build_send(sockfd, client_addr, addr_len,ACK_Response, Cur_SeqNUM);
                    printf("\r\r\nEOF: UPLOAD completed successfully\nResponse sent to client\n\n\n");
                    
                    break;
                }

                // If reach here in the loop: (the order of the if condition is relvant)
                // Building ACK Response and sending to client
                ACK_Build_send(sockfd, client_addr, addr_len,ACK_Response, Cur_SeqNUM);
                printf("\nResponse sent to client\nAwaiting further Data packets.....\n\n");

            } // End of Transfer and Recieving packet and ACK loop

            fclose(Fpoint);
            fclose(Backup_Fpoint);  

        }//END of UPLOAD Handler section 


        // DOWNLOAD Operation Handler section //

        if ( ClientRequest.CurOP_ID == DOWNLOAD )
        {
            // Settin ON Timeout
            tv.tv_sec = TimeoutValue; 
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

            // Handeling client Response for intializing DOWNLOAD 
            ResponseHandleACK(RetryCount ,sockfd, client_addr,  addr_len, 0); // RetryCount -> Timeout will exit


            // Creating a DATA packet from reading file and send it to client
            FILE* Fpoint = NULL; // Itreator traverse file
            u_int16_t Seq_NUM = 1; // Initial packet number
            size_t last_DATApack_size = 0; // helper var
            size_t packet_buf_size = 0; // full data message size

            Fpoint = fopen(ClientRequest.CurOP_FilePATH,"r"); // creating a poineter file to scan the intented file

            // Main Transfer to client loop
            while (1) 
            {
                DataPacket Dpack = {0}; // Create a struct for packet
                Dpack.packet_id = Seq_NUM;

                // Filling up a container to up to a total max of 512(-4) bytes
                Dpack.data_size = fread(Dpack.data, 1, Packet_Max_SIZE-4, Fpoint);


                // Check for EOF - Condition to end loop
                if (Dpack.data_size == 0) {

                    if (feof(Fpoint)) { // Due to EOF
                        printf("\nReached END OF FILE.\n");
                        if (last_DATApack_size == 508) // special case
                            {memset(Dpack.data, 0, Packet_Max_SIZE - 4);;} // give client indication for ending transfer
                        else 
                            {free(Dpack.packet_buf); break;} // End transfer loop
                    
                    } 
                    else if (ferror(Fpoint)) { // Due to ERROR
                        printf("ERROR while reading from file.\n\n");
                        break; //****ERRROr HANDLER */
                    }
                }

                // Building DATA packet message
                Dpack.packet_buf = DATApack_Build(Dpack.data_size, Dpack.data, Dpack.packet_id);
                packet_buf_size = Dpack.data_size + 4;

                int count = 0;
                while (1) // ACK Check LOOP and Retransmission previuos packet 
                {
                    // Sending packet to client
                    sendto(sockfd, Dpack.packet_buf, packet_buf_size, 0,
                        (const struct sockaddr *)&client_addr, sizeof(server_addr));

                    // Handeling client Response for packet Transfer 
                    printf("\nOP_ID & SeqNUM in bytes (%ld):  ",packet_buf_size);
                        for (size_t i = 0; i < 4; i++)
                            {printf("%02X ", Dpack.packet_buf[i]);}
                        printf("\n\n");

                    if (ResponseHandleACK(count ,sockfd, client_addr,  addr_len, Dpack.packet_id))
                        {break;} // Only if recivied the last Seq_NUM NOT Reach here and keep looping
                }

                // Preparing variables for next itertion - Freeing allocated memory
                free(Dpack.packet_buf);
                Seq_NUM++;
                last_DATApack_size = Dpack.data_size;

            }

            fclose(Fpoint);

            // If reach here the DOWNLOAD was successfull and Transfer completed 
            printf ("\n\nDOWNLOAD Request completed\nListening to further Request...\n\n\n");
        
        }// END of UPLOAD Handler     
        
       
    }//END of client Request and Data transfer Handler loop


    close(sockfd);
    return 0;

}//END of main server