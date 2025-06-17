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
#define DOWNLAOD    2
#define DATA        3
#define ACK         4 
#define ERROR       5
#define DELETE      6

#define MAX_BUFFER_SIZE 1024 


/* This function gets pointers to string of OPID and FilePATH
 * Based on that it builds clients Request massage
 */

char *REQ_msg_build(char *Operation_name,char *File_Path, size_t *msg_size){
    
    uint16_t OP_ID;

    // Caculate the OP CODE based on operation name given
    if (strcasecmp(Operation_name, "download") == 0) {
        OP_ID = DOWNLAOD;
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
   
    // Return messgae size to main client
    *msg_size = message_size;

    return message_buffer; // Return the pointer to the allocated buffer
}


/* This function gets the client request in bytes along with pointers to OPID and FilePATH var in main server.
 * Translate the bytes recivied into int for OP_ID and string for FIlePATH
 * Then using the pointers given store in var at the main server
*/

int Convert_REQBytes_String(char REQBytes[MAX_BUFFER_SIZE], size_t * FilePATH_len, int16_t *OPID,char *FilePATH){
    // Finding Operation name ID
    *OPID = (int16_t)REQBytes[1];

    // Finding FilePATH
    char FilePATH_bin[256] = {0};  int i = 2; //Intiliaztions
    
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
    * It recive the intendet Response buffer pointer and packet_SeqNUM
    * The function build the correct response and assign it to given buffer
*/
int ACK_Build(char *Response_buf, int16_t packet_SeqNUM){

    // Converting to network bit order
    int32_t ACK_bytes = htons(ACK); // 16bit > 32bit with zero pad
    packet_SeqNUM = htons(packet_SeqNUM);

    // Copying OP ID bytes to buffer    
    memcpy(Response_buf, &ACK_bytes, sizeof(int32_t));

    // Copying packet_SeqNUM bytes to  lower part of buffer    
    memcpy(Response_buf + 2, &packet_SeqNUM, sizeof(int16_t));

    return 0;
}


/* This function check that client recivied the correct ACK respone for UPLOAD*/
int CompareResponseTOExpectedACK(int bytes, char *Response ){
    
    const int32_t ACK_UPLOAD_VALUE = 0x00040000; // Correct ACK for UPLOAD
    int32_t received_bytes = {0}; // temp var

    if (bytes == 4){ // First check size corelation
        
        // Copy bytes into a 32bit integer variable
        memcpy(&received_bytes, Response, sizeof(int32_t));

        // Convert back to host byte order
        received_bytes = ntohl(received_bytes);

        return (received_bytes == ACK_UPLOAD_VALUE ? 1 : 0);
    }

    else {return 0;}
}


char* DATApack_Build(size_t *pack_size, char *DATApack, int16_t packet_SeqNUM){

    // Converting to network bit order
    int16_t OPID = htons(DATA); 
    packet_SeqNUM = htons(packet_SeqNUM);

    // Calculate the size of the packet
    // { OP ID + Seq Number + DataPacket }
    size_t packet_size = sizeof(int16_t) + sizeof(int16_t) + strlen(DATApack) ;

    // Dynamically allocate memory for the message buffer
    char *packet_buffer = (char *)malloc(packet_size);
    if (packet_buffer == NULL) {
        perror("malloc failed");
        return NULL; // allocation failure
    }

    // Use a pointer to advance along the buffer
    char *current_pos = packet_buffer;

    // Copy the 2 bytes OP_ID to the message buffer
    memcpy(current_pos, &OPID, sizeof(OPID));
    current_pos += sizeof(OPID); // Advance the pointer

    // Copy the 2 bytes SeqNUM to the message buffer
    memcpy(current_pos, &packet_SeqNUM, sizeof(packet_SeqNUM));
    current_pos += sizeof(packet_SeqNUM); // Advance the pointer

    // Copy the Data packet and its null terminator
    strcpy(current_pos, DATApack);
   
    // Return packet size to main
    *pack_size = packet_size;

    return packet_buffer; // Return the pointer to the allocated buffer

}










#endif