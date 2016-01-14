#define MAX_NAME 32
typedef struct participant {
   char name[MAX_NAME];
   char *comment;
   char *availability;
   struct participant *next;
} Participant; 

typedef struct poll {
   char name[MAX_NAME];
   int num_slots;
   char **slot_labels;
   struct poll *next;
   Participant *participants;
} Poll;

/* Add a participant with this part_name to the participant list for the poll
 * with this poll_name in the list at head_pt. Duplicate participant names
 * are not allowed. Set the availability of this participant to avail.
 * Return: 0 on success 
 *         1 for poll does not exist with this name
 *         2 for participant by this name already in this poll
 *         3 for availibility string is wrong length for this poll. 
 *           Particpant is not added. 
 */
int add_participant(char *part_name, char *poll_name, Poll *head_pt, char *avail);


/* Create a poll with this name and num_slots and set the slot labels.
 * Insert it into the list of polls whose head is pointed to by *head_ptr_add
 * Return 0 if successful 
 * Return 1 if a poll by this name already exists in this list.
 */
int create_poll(char *name, char **slot_labels, int num_slots, Poll **head_ptr_add);

/* Return a pointer to the poll with this name in
 * this list starting with head. Return NULL if no such poll exists.
 */
Poll *find_poll(char *name, Poll *head); 

/* Delete the poll by the name poll_name from the list at *head_ptr_add.
 * Update the head of the list as appropriate and free all dynamically 
 * allocated memory no longer used.
 * Return 0 if successful and 1 if poll by this name is not found in list. 
 */
int delete_poll(char *poll_name, Poll **head_ptr_add);

/* helper for delete_poll */
//void free_memory(Poll *poll);

/*  Return pointer to participant with this name from this poll or
 *  NULL if no such participant exists.
 */
Participant *find_part(char *name, Poll *poll); 


/* Add a comment from the participant with this part_name to the poll with
 * this poll_name. Replace existing comment if one exists. 
 * Return values:
 *    0 on success
 *    1 no poll by this name
 *    2 no participant by this name for this poll
 */
int add_comment(char *part_name, char *poll_name, char *comment, Poll *head_pt);


/* Add availabilty for the participant with this part_name to the poll with
 * this poll_name. Return values:
 *    0 success
 *    1 no poll by this name
 *    2 no participant by this name for this poll
 *    3 avail string is incorrect size for this poll 
 */
int update_availability(char *part_name, char *poll_name, char *avail, 
                     Poll *head_pt);



/* 
 *  Print the names of the current polls one per line.
 */
char* print_polls(Poll *head);


/* For the poll by the name poll_name from the list at head_ptr,
 * print the name each label and each participant.
 * For each participant, print name and availability.
 * Return 0 if successful and 1 if poll by this name is not found in list. 
 */
char* print_poll_info(char *poll_name, Poll *head);

