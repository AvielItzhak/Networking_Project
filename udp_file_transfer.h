#ifndef udp_file_transfer
#define udp_file_transfer

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <errno.h>
#include <arpa/inet.h>


/* OP_ID */
#define UPLOAD      1 
#define DOWNLOAD    2
#define DATA        3
#define ACK         4 
#define ERROR       5
#define DELETE      6

#define MAX_BUFFER_SIZE 1024 
#define Packet_Max_SIZE 512
#define TimeoutValue 10
#define RetryCount 3



typedef struct {
    char OperationNAME[9];
    char FilePATH[256];
    char FileNAME[128];
    char OP_Detail[256];
} Requestinfo;

typedef struct {
    u_int16_t packet_id;
    char data[Packet_Max_SIZE];
    size_t data_size;
    unsigned char *packet_buf;
} DataPacket;

typedef struct {// Request INFO
    int16_t CurOP_ID;
    char CurOP_FilePATH[256];
    size_t FilePATH_len;
    char CurOP_Detail[256];
} CurrRequestINFO;






// Check argument Error - Typo and missing file while uploading
void CheckArgError (char OperationNAME[9], char FilePATH[128])
{
    if (strcasecmp(OperationNAME, "delete") != 0  
        && strcasecmp(OperationNAME, "download") != 0 
        && strcasecmp(OperationNAME, "upload") != 0) {

        perror("Wrong input...\n");
        exit(EXIT_FAILURE);
    }
    if (access(FilePATH, F_OK) != 0 && strcasecmp(OperationNAME, "upload") == 0) {
        fprintf(stderr, "File '%s' not found.\n\n", FilePATH);
        exit(EXIT_FAILURE);
    }
    if (access(FilePATH, R_OK) != 0 && strcasecmp(OperationNAME, "upload") == 0) {
        fprintf(stderr, "File '%s' doesn't have read permission.\n\n", FilePATH);
        exit(EXIT_FAILURE);
    }
}


/* This function gets pointers to string of OPID and FilePATH
 * Based on that it builds clients Request massage
 */

char *REQ_msg_build(char *Operation_name,char *File_Path, size_t *msg_size){
    
    uint16_t OP_ID;

    // calculate the OP CODE based on operation name given
    if (strcasecmp(Operation_name, "download") == 0) {
        OP_ID = DOWNLOAD;
    } else if (strcasecmp(Operation_name, "delete") == 0) {
        OP_ID = DELETE;
    } else if (strcasecmp(Operation_name, "upload") == 0) {
        OP_ID = UPLOAD;
    } else {
        // Error invalid Operation name
        fprintf(stderr, "Error Invalid Operation name: %s\n", Operation_name);
        return NULL;
    }

    // Convert the OP_ID to 2 bytes
    uint16_t Twobytes_OP_ID = htons(OP_ID);

    // Calculate the size of the message
    // { Opcode (2 bytes) + FilePATH + Null Terminator + Mode + Null Terminator }
    size_t message_size = sizeof(uint16_t) + strlen(File_Path) + 1 + strlen("netascii") + 1 ;

    // Dynamically allocate memory for the message buffer
    char *message_buffer = (char *)malloc(message_size);
    if (message_buffer == NULL) {
        perror("malloc failed");
        return NULL; // allocation failure
    }

    // Use a pointer to advance along the buffer
    char *current_pos = message_buffer;

    // Copy the 2 bytes OP_ID to the message buffer
    memcpy(current_pos, &Twobytes_OP_ID, sizeof(Twobytes_OP_ID));
    current_pos += sizeof(Twobytes_OP_ID); // Advance the pointer

    // Copy the file_path and its null terminator
    strcpy(current_pos, File_Path);
    current_pos += strlen(File_Path) + 1; // Move the pointer past the filename and null terminator

    // Copy the mode and its null terminator
    strcpy(current_pos, "netascii");
   
    // Return message size to main client
    *msg_size = message_size;

    return message_buffer; // Return the pointer to the allocated buffer
}


/* This function gets the client request in bytes along with pointers to OPID and FilePATH var in main server.
 * Translate the bytes received into int for OP_ID and string for FIlePATH
 * Then using the pointers given store in var at the main server
*/

int Convert_REQBytes_String(unsigned char REQBytes[MAX_BUFFER_SIZE], size_t * FilePATH_len, int16_t *OPID,char *FilePATH){
    // Finding Operation name ID
    *OPID = (int16_t)REQBytes[1];

    // Finding FilePATH
    char FilePATH_bin[256] = {0};  int i = 2; //initializations
    
    for (; i < MAX_BUFFER_SIZE && REQBytes[i] != '\0'; i++)
    {
        FilePATH_bin[i-2] = REQBytes[i];
    }

    strcpy(FilePATH,FilePATH_bin); // Copying temp values to FilePATH var in main server
    *FilePATH_len = (size_t) i-2; // assign values to FilePATH_len var in main server 
    
    return (int16_t)REQBytes[1]; // return the OPID
}



/* This function gets the FilePATH from the server and delete it 
 * In case of Error a string sent to var in the main server
 */
int RequestHandler_Delete(char *FilePATH, char * Detail){
    if (remove(FilePATH) == 0) {
        // In case of Successes return positive feedback back to main server
        sprintf(Detail,"File '%s' successfully deleted.", FilePATH);
    } 
    else {
        // In case of Failure return Error detail feedback back to main server
        sprintf(Detail,"Error deleting file: %s", strerror(errno));
    }
   return 0;
}



/* This function create and send 32bit ACK Response for Upload Request and DATA packet
    * It receive the intended Response buffer pointer and packet_SeqNUM
    * The function build the correct response and assign it to given buffer
*/
int ACK_Build(unsigned char *Response_buf, u_int16_t packet_SeqNUM){

    // Converting to network bit order
    int32_t ACK_bytes = htons(ACK); // 16bit > 32bit with zero pad
    packet_SeqNUM = htons(packet_SeqNUM);

    // Copying OP ID bytes to buffer    
    memcpy(Response_buf, &ACK_bytes, sizeof(ACK_bytes));

    // Copying packet_SeqNUM bytes to  lower part of buffer    
    memcpy(Response_buf + 2, &packet_SeqNUM, sizeof(packet_SeqNUM));

    return 0;
}

/* A complentry function to send ACK_build output*/
int ACK_Build_send(int sockfd, struct sockaddr_in client_addr, socklen_t addr_len,unsigned char *ACK_Response, u_int16_t packet_SeqNUM){

    // Building ACK Response and sending to client
    ACK_Build(ACK_Response, packet_SeqNUM);
    
    // Sending 4byte ACK _Response to client
    sendto(sockfd, ACK_Response, sizeof(ACK_Response), 0,
                            (const struct sockaddr *)&client_addr, addr_len);

    return 0;
}


/* This function check that client redivide the correct ACK response for UPLOAD*/
int CompareResponseTOExpectedACK(int bytes, unsigned char *Response, u_int16_t Exp_Seq_NUM){
    
    int16_t ACK_VALUE = 0x0004; // Correct ACK for UPLOAD
    u_int16_t Seq_NUM_Rec_bytes = {0}; // temp var
    int16_t OPID_Rec_bytes = {0}; // temp var

    // Convert to network byte order
    Exp_Seq_NUM = ntohs(Exp_Seq_NUM);

    // Copy the first 2 bytes into a 16bit integer variable
    memcpy(&OPID_Rec_bytes, Response, sizeof(OPID_Rec_bytes));

    // Convert to host byte order
    OPID_Rec_bytes = ntohs(OPID_Rec_bytes);

    if (bytes == 4 && OPID_Rec_bytes == ACK_VALUE) { // First check size corelation and OPID is ACK
        
        // Copy the last 2 bytes into a 16bit integer variable
        memcpy(&Seq_NUM_Rec_bytes, Response + 2, sizeof(Seq_NUM_Rec_bytes));
        if (Seq_NUM_Rec_bytes == 0) // First Request 0
            {return Seq_NUM_Rec_bytes == Exp_Seq_NUM ? 1 : 0;}
        
        if (Seq_NUM_Rec_bytes == Exp_Seq_NUM ? 1 : 0) // Right Seq_NUM
            {return 2;}
        else  // The last Seq_NUM
            {return Seq_NUM_Rec_bytes == Exp_Seq_NUM - 1 ? -1 : 0;}
    }

    else {return 0;} // If not the right size

} 



/* This function receive Response from and check ERRORS and Correct Response */ 
int ResponseHandleACK(int count , int sockfd, struct sockaddr_in addr,  socklen_t addr_len, int16_t Seq_NUM) {
       
    int Response = 0; // initialized Loop condition
    unsigned char ACK_Resp_buf[4] = {0}; 

    // receiving bytes
    Response = recvfrom(sockfd, ACK_Resp_buf, sizeof(ACK_Resp_buf), 0,
                                    (struct sockaddr *)&addr, &addr_len);

    // Check ERROR and Timeout
    if (Response < 4)
    {
        // Check Timeout ERROR
        if (errno == EAGAIN || errno == EWOULDBLOCK) { 
            perror("\nTimeout occurred while waiting for Response");
            if (count == RetryCount) // Incase Retransmit packet" RetryCount" times and reached Timeout
            {
                printf("\nEnding Transfer session now due to timeout\n\n");
                exit (errno); 
            }
            // Incase didn't Retransmit packet "RetryCount" times. 
            count ++;
            printf("\nRetransmiting packet...\n\n");
            return 0; // KEEP ACK_LOOP 
            
        }
        else // Other ERRORS
        {
            perror("Error Receiving message\n"); // Print Error detail in server terminal
            printf("\n*Didn't* Got Feedback, Request finished and client will close\n\n");
            exit (errno);
        }    
    }   

    else 
    { // In the case of incoming bytes print Response
        printf("\nResponse:\n");
            for (int i = 0; i < Response; i++)
                {printf("%02X ", ACK_Resp_buf[i]);}
            printf("\n\n");

        // Checking for correct ACK Response bytes - Right ACK || LAST SeqNUM || Unknown ACK
        if (CompareResponseTOExpectedACK(Response, ACK_Resp_buf, Seq_NUM) > 0)
        {
            printf("\nCheck: Correct ACK, Initiating packet Transfer\n\n");
            return 1; // Break ACK_LOOP
        }

        if (CompareResponseTOExpectedACK(Response, ACK_Resp_buf, Seq_NUM) < 0)
        {
            printf("\nCheck: NOT Correct ACK, Resending Last packet\n\n");
            return 0; // KEEP ACK_LOOP
        }

        else 
        {
            printf("\nCheck: Unknown Response, Request finished and client will close\n\n");
            exit (EXIT_FAILURE); // Ending Transfer and exiting program with failure
        }
    }
    return -1;
}




/* This function gets DATA and SeqNUM and build a DATA message*/

unsigned char* DATApack_Build(size_t DATAsize, char *DATApack, u_int16_t packet_SeqNUM){

    // Converting to network bit order
    int16_t OPID = htons(DATA); 
    packet_SeqNUM = htons(packet_SeqNUM);

    // Calculate the size of the packet
    // { OP ID + Seq Number + DataPacket }
    size_t packet_size = DATAsize + 4 ;

    // Dynamically allocate memory for the message buffer
    unsigned char *packet_buffer = (unsigned char *)malloc(packet_size);
    if (packet_buffer == NULL) {
        perror("malloc failed");
        return NULL; // allocation failure
    }

    // Use a pointer to advance along the buffer
    unsigned char *current_pos = packet_buffer;

    // Copy the 2 bytes OP_ID to the message buffer
    memcpy(current_pos, &OPID, sizeof(OPID));
    current_pos += sizeof(OPID); // Advance the pointer

    // Copy the 2 bytes SeqNUM to the message buffer
    memcpy(current_pos, &packet_SeqNUM, sizeof(packet_SeqNUM));
    current_pos += sizeof(packet_SeqNUM); // Advance the pointer

    // Copy the Data packet and its null terminator
    memcpy(current_pos, DATApack,DATAsize);
   
    return packet_buffer; // Return the pointer to the allocated buffer

}










#endif