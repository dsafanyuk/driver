/**********************************************************************/
/*                                                                    */
/* Program Name: driver - Demonstrate a disk device driver            */
/* Author:       Dave Safanyuk                                        */
/* Installation: Pensacola Christian College, Pensacola, Florida      */
/* Course:       CS326, Operating Systems                             */
/* Date Written: April 16, 2019                                       */
/*                                                                    */
/**********************************************************************/

/**********************************************************************/
/*                                                                    */
/* I pledge  the C language  statements in  this  program are  my own */
/* original  work,  and none  of the  C language  statements in  this */
/* program were copied  from any one else,  unless I was specifically */
/* authorized to do so by my CS326 instructor.                        */
/*                                                                    */
/*                                                                    */
/*                           Signed: ________________________________ */
/*                                             (signature)            */
/*                                                                    */
/**********************************************************************/

/**********************************************************************/
/*                                                                    */
/* This program creates a file system request list and a pending      */
/* request list, puts read/write requests from file system to pending */
/* request list, convert physical block numbers into disk drive       */
/* cylinder, track, and sector numbers, then tell the disk device to  */
/* process read/write requests.                                       */
/*                                                                    */
/**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**********************************************************************/
/*                         Symbolic Constants                         */
/**********************************************************************/
#define CYLINDERS_PER_DISK 40   /* Number of cylinders in a disk      */
#define TRACKS_PER_CYLINDER 2   /* Number of tracks in a cylinder     */
#define SECTORS_PER_TRACK 9     /* Number of sectors in a track       */
#define BYTES_PER_SECTOR 512    /* Number of bytes in a sector        */
#define SECTORS_PER_BLOCK 2     /* Number of sectors in a block       */
#define LIST_HEADER 0           /* Linked list header value           */
#define LIST_TRAILER 361        /* Linked list trailer value          */
#define HEADER_ALLOC_ERR 1      /* Header memory allocation error     */
#define TRAILER_ALLOC_ERR 2     /* Trailer memory allocation error    */
#define REQUEST_ALLOC_ERR 3     /* Request memory allocation error    */
#define MAX_REQUEST_NUM 32767   /* Maximum request number allowed     */
#define MAX_IDLE_REQUESTS 2     /* Maximum idle requests allowed      */
#define SENSE_CYLINDER 1        /* Sense cylinder code number         */
#define SEEK_TO_CYLINDER 2      /* Seek to cylinder code number       */
#define DMA_SETUP 3             /* DMA setup code number              */
#define START_MOTOR 4           /* Start motor code number            */
#define STATUS_MOTOR 5          /* Status motor code number           */
#define READ_DATA 6             /* Read data code number              */
#define WRITE_DATA 7            /* Write data code number             */
#define STOP_MOTOR 8            /* Stop motor code number             */
#define RECALIBRATE 9           /* Recalibrate code number            */
#define MAX_PENDING_REQUESTS 20 /* Maximum pending requests allowed   */

/**********************************************************************/
/*                         Program Structures                         */
/**********************************************************************/
/* A file system request list entry                                   */
struct message
{
    int operation_code;                /* The disk operation to be    */
                                       /* performed                   */
    int request_number;                /* A unique request number     */
    int block_number;                  /* The block number to be read */
                                       /* or written                  */
    int block_size;                    /* The block size in bytes     */
    unsigned long int *p_data_address; /* Points to the data block in */
                                       /* memory                      */
};
typedef struct message MESSAGE;
MESSAGE fs_message[MAX_PENDING_REQUESTS]; /* File system request list  */

/* A pending request list entry                                        */
struct request
{
    int block_number,                  /* The block number to be read  */
                                       /* or written                   */
        block_size,                    /* The block size in bytes      */
        operation_code,                /* The disk operation to be     */
                                       /* performed                    */
        request_number;                /* A unique request number      */
    unsigned long int *p_data_address; /* Points to the data block in  */
                                       /* memory                       */
    struct request *p_next_request;    /* Points to the next request   */
};
typedef struct request REQUEST;
REQUEST *p_pending_request_list; /* Points to the pending request     */
                                 /* list                              */

/**********************************************************************/
/*                        Function Prototypes                         */
/**********************************************************************/
void convert_block(int block, int *p_cylinder, int *p_sector, int *p_track);
/* Convert physical block numbers into disk drive cylinder, track,    */
/* and sector numbers                                                 */
REQUEST *create_list();
/* Create an empty pending request list with header and trailer       */
REQUEST *create_pending_request(MESSAGE fs_message);
/* Create a new pending request and return its address                */
void add_pending_request(REQUEST *p_pending_request_list,
                         REQUEST *p_new_request);
/* Add a new pending request and put it in order by block number      */
void remove_pending_request(REQUEST *p_current_request,
                            REQUEST *p_pending_request_list);
/* Remove the given request from the pending request list             */
void set_idle_message(MESSAGE *fs_message);
/* Check for any invalid parameters and return the error code         */
int get_error_code(REQUEST *p_current_request);
/* Set an idle message                                                */

/**********************************************************************/
/*                           Main Function                            */
/**********************************************************************/
int main()
{
    REQUEST *p_current_request;  /* Points to the current request     */
    bool disk_on = false;        /* Disk drive status                 */
    int error_code = 0,          /* Error code number                 */
        count_fs_message = 0,    /* Count file system request         */
                                 /* list entries                      */
        count_idle = 0,          /* Count idle requests               */
        disk_heads,              /* Current disk heads' position      */
                                 /* in cylinder number                */
        cylinder,                /* Cylinder number                   */
        last_request_number = 0, /* Last request number from the      */
                                 /* file system                       */
        sector,                  /* Sector number                     */
        track,                   /* Track number                      */
        value_sign = 1;          /* Multiplier to change sign         */

    /* Create empty pending request list                              */
    p_pending_request_list = create_list();

    /* Loop processing the driver, never stops                        */
    while (true)
    {
        /* Loop processing file system messages                       */
        while (fs_message[count_fs_message].operation_code != 0 &&
               count_fs_message < MAX_PENDING_REQUESTS)
        {
            /* Set last request number to zero everytime it reaches   */
            /* maximum request number                                 */
            if (last_request_number == MAX_REQUEST_NUM)
                last_request_number = 0;

            /* Add a request into pending request list                */
            if (fs_message[count_fs_message].request_number > last_request_number ||
                fs_message[count_fs_message].request_number <= 0)
            {
                add_pending_request(p_pending_request_list,
                                    create_pending_request(fs_message[count_fs_message]));

                if (fs_message[count_fs_message].request_number != 0)
                {
                    last_request_number =
                        fs_message[count_fs_message].request_number;
                }
            }

            count_fs_message++;
        }
        count_fs_message = 0;

        /* Check if pending request list have any request to process  */
        if (p_pending_request_list->p_next_request->p_next_request != NULL)
        {
            /* Turn the disk drive motor on if it is off              */
            count_idle = 0;
            if (disk_on == false)
            {
                disk_on = disk_drive(START_MOTOR, 0, 0, 0, 0);

                /* Loop with empty body, wait until the disk motor is */
                /* up to speed                                        */
                while (disk_drive(STATUS_MOTOR, 0, 0, 0, 0) != 0)
                    ;

                /* Sense and set the disk heads current cylinder position */
                disk_heads = disk_drive(SENSE_CYLINDER, 0, 0, 0, 0);
            }

            /* Choose the next request to process using a disk arm    */
            /* elevator scheduling algorithm                          */
            p_current_request = p_pending_request_list->p_next_request;
            while (convert_block(p_current_request->block_number, &cylinder,
                                 &sector, &track),
                   (disk_heads * value_sign) >
                       (cylinder * value_sign))
            {
                if (p_current_request->p_next_request->p_next_request == NULL)
                {
                    value_sign = value_sign * -1;
                    p_current_request = p_pending_request_list->p_next_request;
                }
                else
                {
                    p_current_request = p_current_request->p_next_request;
                }
            }

            /* Change the multiplier to positive                      */
            if (value_sign == -1)
                value_sign = 1;

            /* Get and set the error code                             */
            error_code = get_error_code(p_current_request);

            /* Process the request if there is no errors              */
            if (error_code == 0)
            {
                /* Check if the request to process is in another cylinder */
                if (disk_heads != cylinder)
                {
                    /* Send the disk heads to the given cylinder      */
                    disk_heads = disk_drive(SEEK_TO_CYLINDER, 
                                                     cylinder, 0, 0, 0);

                    /* Check if the disk heads land correctly         */
                    while (disk_heads != cylinder)
                    {
                        /* Loop with empty body, wait until the disk  */
                        /* heads land on cylinder zero                */
                        while (disk_drive(RECALIBRATE, 0, 0, 0, 0) != 0)
                            ;

                        disk_heads = 0;

                        if (cylinder != 0)
                        {
                            disk_heads =
                                disk_drive(SEEK_TO_CYLINDER, 
                                                    cylinder, 0, 0, 0);
                        }
                    }
                }

                /* Process the request if DMA sets up correctly       */
                if (disk_drive(DMA_SETUP, sector, track, 
                               BYTES_PER_SECTOR * SECTORS_PER_BLOCK, 
                               p_current_request->p_data_address) == 0)
                {
                    /* Read or write the request                      */
                    if (p_current_request->operation_code == 1)
                    {
                        while (disk_drive(READ_DATA, 0, 0, 0, 0) != 0)
                            ;

                        fs_message[0].operation_code = 0;
                    }
                    else
                    {
                        while (disk_drive(WRITE_DATA, 0, 0, 0, 0) != 0)
                            ;

                        fs_message[0].operation_code = 0;
                    }
                }
            }
            else
            {
                /* Set the error code to be send to the file system   */
                fs_message[0].operation_code = error_code;
                error_code = 0;
            }

            /* Set a message and send it to the file system           */
            fs_message[0].request_number = p_current_request->request_number;
            fs_message[0].p_data_address = p_current_request->p_data_address;
            fs_message[0].block_number = p_current_request->block_number;
            fs_message[0].block_size = p_current_request->block_size;
            send_message(fs_message);

            /* Remove the request from the pending request list       */
            remove_pending_request(p_current_request, p_pending_request_list);
        }
        else
        {
            /* Turn the disk drive motor off after receiving two idle */
            /* request in a row from the file system                  */
            count_idle += 1;
            if (count_idle >= MAX_IDLE_REQUESTS && disk_on)
            {
                disk_drive(STOP_MOTOR, 0, 0, 0, 0);
                disk_on = false;
            }

            /* Set an idle message and send it to the file system     */
            set_idle_message(&fs_message[0]);
            send_message(fs_message);
        }
    }

    return 0;
}

/**********************************************************************/
/*  Convert physical block numbers into disk drive cylinder, track,   */
/*                         and sector numbers                         */
/**********************************************************************/
void convert_block(int block, int *p_cylinder, int *p_sector, int *p_track)
{
    *p_cylinder = (int)((block - 1) / SECTORS_PER_TRACK);

    if (((block - 1) % SECTORS_PER_TRACK) <=
        (SECTORS_PER_TRACK / TRACKS_PER_CYLINDER))
        *p_track = 0;
    else
        *p_track = 1;

    *p_sector = ((block - 1) % SECTORS_PER_TRACK) * SECTORS_PER_BLOCK;

    if (*p_sector > SECTORS_PER_TRACK)
        *p_sector = *p_sector % SECTORS_PER_TRACK;

    return;
}

/**********************************************************************/
/*    Create an empty pending request list with header and trailer    */
/**********************************************************************/
REQUEST *create_list()
{
    REQUEST *p_new_list; /* Points to the new pending request list    */

    if ((p_new_list = (REQUEST *)malloc(sizeof(REQUEST))) == NULL)
    {
        printf("\nError #%d occurred in create_list.", HEADER_ALLOC_ERR);
        printf("\nUnable to allocate memory for the list header.");
        printf("\nThe program is aborting.");
        exit(HEADER_ALLOC_ERR);
    }

    p_new_list->block_number = LIST_HEADER;

    if ((p_new_list->p_next_request = (REQUEST *)malloc(sizeof(REQUEST))) == NULL)
    {
        printf("\nError #%d occurred in create_list.", TRAILER_ALLOC_ERR);
        printf("\nUnable to allocate memory for the list trailer.");
        printf("\nThe program is aborting.");
        exit(TRAILER_ALLOC_ERR);
    }

    p_new_list->p_next_request->block_number = LIST_TRAILER;
    p_new_list->p_next_request->p_next_request = NULL;

    return p_new_list;
}

/**********************************************************************/
/*       Create a new pending request and return its address          */
/**********************************************************************/
REQUEST *create_pending_request(MESSAGE fs_message)
{
    REQUEST *p_new_request; /* Points to the new request              */

    if ((p_new_request = (REQUEST *)malloc(sizeof(REQUEST))) == NULL)
    {
        printf("\nError #%d occurred in create_pending_request.",
               REQUEST_ALLOC_ERR);
        printf("\nUnable to allocate memory for a new request.");
        printf("\nThe program is aborting.");
        exit(REQUEST_ALLOC_ERR);
    }

    p_new_request->block_number = fs_message.block_number;
    p_new_request->block_size = fs_message.block_size;
    p_new_request->operation_code = fs_message.operation_code;
    p_new_request->p_data_address = fs_message.p_data_address;
    p_new_request->request_number = fs_message.request_number;
    p_new_request->p_next_request = NULL;

    return p_new_request;
}

/**********************************************************************/
/*   Add a new pending request and put it in order by block number    */
/**********************************************************************/
void add_pending_request(REQUEST *p_pending_request_list,
                         REQUEST *p_new_request)
{
    if (p_pending_request_list->p_next_request->p_next_request != NULL)
        while (p_pending_request_list->p_next_request->block_number <=
               p_new_request->block_number)
            p_pending_request_list = p_pending_request_list->p_next_request;

    p_new_request->p_next_request = p_pending_request_list->p_next_request;
    p_pending_request_list->p_next_request = p_new_request;

    return;
}

/**********************************************************************/
/*      Remove the given request from the pending request list        */
/**********************************************************************/
void remove_pending_request(REQUEST *p_current_request,
                            REQUEST *p_pending_request_list)
{
    REQUEST *p_current, /* Points to every request                    */
        *p_previous;    /* Points to the previous request             */

    p_previous = p_pending_request_list;
    while (p_current = p_previous->p_next_request,
           p_current->request_number != p_current_request->request_number)
        p_previous = p_current;

    p_previous->p_next_request = p_current->p_next_request;
    free(p_current);

    return;
}

/**********************************************************************/
/*     Check for any invalid parameters and return the error code     */
/**********************************************************************/
int get_error_code(REQUEST *p_current)
{
    int error_code = 0; /* The error code to be returned              */

    if (p_current->operation_code != 1 &&
        p_current->operation_code != 2)
        error_code -= 1;

    if (p_current->request_number < 1)
        error_code -= 2;

    if (!(p_current->block_number >= 1 &&
          p_current->block_number <= (CYLINDERS_PER_DISK * SECTORS_PER_TRACK)))
        error_code -= 4;

    if ((p_current->block_size % 2) != 0 ||
        p_current->block_size < 0 ||
        p_current->block_size > (BYTES_PER_SECTOR * SECTORS_PER_TRACK *
                                 TRACKS_PER_CYLINDER))
        error_code -= 8;

    if (p_current->p_data_address < 0)
        error_code -= 16;

    return error_code;
}

/**********************************************************************/
/*                         Set an idle message                        */
/**********************************************************************/
void set_idle_message(MESSAGE *fs_message)
{
    (*fs_message).operation_code = (*fs_message).request_number =
        (*fs_message).block_number = (*fs_message).block_size = 0;
    (*fs_message).p_data_address = NULL;

    return;
}
