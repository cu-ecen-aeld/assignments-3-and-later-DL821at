/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer implementation
 *
 * This version explicitly uses the 'full' flag in struct aesd_circular_buffer
 * to manage overwrite logic. The ring is considered full if incrementing
 * in_offs causes it to match out_offs. If full, the oldest entry at out_offs
 * is overwritten, so out_offs is incremented too.
 *
 * The find function iterates over only the valid entries in the ring,
 * which is either the entire buffer if full == true, or the difference
 * between in_offs and out_offs if full == false.
 *
 * This approach ensures we do NOT overwrite the earliest entries until
 * the buffer is actually full, preventing the "Expected 'write1\n' Was 'write2\n'"
 * type errors.
 *
 * Author: Dan Walkes, with modifications for assignment
 * Date: 2020-03-01
 */

 #ifdef __KERNEL__
 #include <linux/string.h>
 #else
 #include <string.h>
 #endif
 
 #include "aesd-circular-buffer.h"
 
 /**
  * @param buffer the buffer to search for corresponding offset.
  *        Any necessary locking must be performed by caller.
  * @param char_offset the position to search for in the buffer list, describing
  *        the zero referenced character index if all buffer strings were
  *        concatenated end to end.
  * @param entry_offset_byte_rtn is a pointer specifying a location to store
  *        the byte offset (within the returned aesd_buffer_entry's buffptr)
  *        corresponding to char_offset.  This value is only set when a
  *        matching char_offset is found in aesd_buffer.
  * @return the struct aesd_buffer_entry structure representing the position
  *         described by char_offset, or NULL if this position is not available
  *         in the buffer (not enough data is written).
  */
 struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
     struct aesd_circular_buffer *buffer,
     size_t char_offset,
     size_t *entry_offset_byte_rtn)
 {
     size_t current_offset = 0;
     size_t entry_count = 0; // how many valid entries in the ring
     uint8_t index;
 
     // If the buffer is full, all AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED entries are valid
     // If not full, the number of valid entries is how many we wrote
     if (buffer->full) {
         entry_count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     } else {
         // in_offs >= out_offs => entries = in_offs - out_offs
         // in_offs < out_offs  => wrap around => in_offs + (SIZE - out_offs)
         entry_count = (buffer->in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
                        - buffer->out_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
 
     // Start from the oldest entry at out_offs
     index = buffer->out_offs;
     for (size_t i = 0; i < entry_count; i++) {
         struct aesd_buffer_entry *entry = &buffer->entry[index];
         size_t entry_size = entry->size;
         // Check if char_offset lies within this entry
         if (char_offset < current_offset + entry_size) {
             // Found the entry containing our offset
             *entry_offset_byte_rtn = char_offset - current_offset;
             return entry;
         }
         current_offset += entry_size;
         index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
 
     // If we exhaust the loop without finding the offset, it's not in the buffer
     return NULL;
 }
 
 /**
  * Adds entry @param add_entry to @param buffer in the location specified
  * in buffer->in_offs.
  *
  * If the buffer was already full, overwrites the oldest entry and advances
  * buffer->out_offs to the new start location.
  *
  * Any necessary locking must be handled by the caller.
  * Any memory referenced in @param add_entry must be allocated by and/or
  * must have a lifetime managed by the caller.
  */
 void aesd_circular_buffer_add_entry(
     struct aesd_circular_buffer *buffer,
     const struct aesd_buffer_entry *add_entry)
 {
     // Place the new entry at in_offs
     buffer->entry[buffer->in_offs] = *add_entry;
 
     // If the buffer is currently full, that means we are overwriting the oldest entry
     if (buffer->full) {
         // Move out_offs forward to drop the oldest entry
         buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
     }
 
     // Advance in_offs
     buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
 
     // If in_offs wrapped around and caught up to out_offs, the buffer is now full
     if (buffer->in_offs == buffer->out_offs) {
         buffer->full = true;
     } else {
         buffer->full = false;
     }
 }
 
 /**
  * Initializes the circular buffer described by @param buffer to an empty struct
  */
 void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
 {
     memset(buffer, 0, sizeof(struct aesd_circular_buffer));
     // We start with an empty buffer, so full = false
     buffer->full = false;
 }
 