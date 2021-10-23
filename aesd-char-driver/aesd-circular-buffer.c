/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer. 
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
			size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
	
	// Do a NULL check on the buffer
	if (buffer == NULL) return NULL;

	uint8_t index = buffer->out_offs;
	uint64_t offset = (uint64_t)buffer->entry[index].size;



	while ((char_offset / offset) != 0)
	{

		index += 1;
		index = index % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		// if the current index value is greater that in_offs
		// then that location has not been written yet.
		if ((is_full(buffer) == false) && (index > buffer->in_offs))
		{
			
			return NULL;
		}

		if ((is_full(buffer) == true) && (index == buffer->in_offs))
		{
			
			return NULL;
		}		
		
		offset += buffer->entry[index].size;
	}


	offset = char_offset - (offset - buffer->entry[index].size);
	
	*entry_offset_byte_rtn = offset;

    return &(buffer->entry[index]);
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description 
    */

	if (buffer == NULL || add_entry == NULL) return;

	buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
	buffer->entry[buffer->in_offs].size = add_entry->size;	

	if (is_full(buffer) == true)
	{
		buffer->full = true;
		buffer->out_offs  = ((buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
	}
	else
	{
		buffer->full = false;	
		buffer->len++;	
	}
	
	buffer->in_offs = ((buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);

   
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

bool is_empty(struct aesd_circular_buffer *buffer)
{

	if (buffer->len == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool is_full(struct aesd_circular_buffer *buffer)
{

	if (buffer->len == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
	{
		return true;
	}
	else
	{
		return false;
	}
}
