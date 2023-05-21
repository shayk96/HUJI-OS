#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <algorithm>

#define SUCCESSES 1
#define FAILURE 0
#define DEFAULT -1


/***
 * an helper function for the DFS
 * @param address start address
 * @param value variable for the PMread
 * @param cur_depth the current depth in the tree's traversing
 * @param tree_depth the depth of the tree
 * @param max the maximum frame index that used in the tree
 * @param empty_table a pointer to insert to the index of the empty table, if exist.
 * @param found_empty a boolean variable indicates weather an empty table was found.
 * @param cur_adder the current frame (for not return the current frame as empty frame)
 * @param prev_adder  a pointer to insert to the previous frame
 * @return the maximum frame index that used in the tree.
 */
int DFS_helper(int address, int value, int cur_depth, int tree_depth, int max, int* empty_table, bool found_empty,
               int cur_adder, int *prev_adder){
  if (*empty_table != DEFAULT) {
    return max;
  }
  bool no_zero = false;
  for (int offset = 0; offset < PAGE_SIZE; offset++){
    PMread(address + offset, &value);
    if (value > max && *empty_table == DEFAULT) {
      max = value;
      no_zero = true;
      *prev_adder = address + offset;
    }
    if (cur_depth == tree_depth - 1) {
      if (value != 0) {
        no_zero = true;
      }
      continue;
    }
    if (value != 0 && *empty_table == DEFAULT) {
      *prev_adder = address + offset;
      max = DFS_helper(value * PAGE_SIZE, value, cur_depth + 1, tree_depth, max, empty_table,
                       found_empty, cur_adder, prev_adder);
      no_zero = true;
    }
  }
  if (!no_zero && (address/PAGE_SIZE) != cur_adder) {
    *empty_table = address/PAGE_SIZE;
    max = DEFAULT;
  }
  return max;
}

/***
 * DFS traversing in order to find an empty table or the maximum frame index used in the tree.
 * @param tree_depth the depth of the tree
 * @param empty_table a pointer to insert to the index of the empty table, if exist.
 * @param cur_adder the current frame (for not return the current frame as empty frame)
 * @param prev_adder a pointer to insert to the previous frame
 * @return the maximum frame index that used in the tree.
 */
int DFS(int tree_depth, int* empty_table, int cur_adder, int* prev_adder){
  int address = 0, value = 0, cur_depth = 0, max = 0;
  bool found_empty = false;
  return DFS_helper(address, value, cur_depth, tree_depth, max, empty_table, found_empty, cur_adder, prev_adder);
}

/***
 *
 * @param max the maximum value calculated
 * @param max_leaf_address a pointer to insert to the address of the chosen leaf address
 * @param vitrual_address the virtual address
 * @param shift_size the size of the segments in the virtual address
 * @param cur_lvl the current depth in the tree's traversing
 * @param max_lvl the depth of the tree
 * @param num_pages the total number of pages in the virtual memory
 * @param offsetless_address the virtual address without the offset
 * @param physical_adr the physical address of the current running
 * @param prev_adr a pointer to insert to the previous frame
 * @param frame_to_evict a pointer to insert to the number of the frame to evict
 * @return the maximum value
 */
int DFS_for_leaf_helper(int max, uint64_t *max_leaf_address, uint64_t vitrual_address, int shift_size, int cur_lvl,
                        int max_lvl, int num_pages, uint64_t offsetless_address, int physical_adr,
                        int* prev_adr, uint64_t* frame_to_evict){
  for (int entry= 0; entry< PAGE_SIZE; entry++){
    int value = 0;
    PMread(physical_adr+entry,&value);
    if (cur_lvl == max_lvl){
      for (int offset= 0; offset < PAGE_SIZE; offset ++){
        int leaf_address = (vitrual_address<< shift_size) + offset;
        PMread(physical_adr+offset,&value);
        if (value != 0){
          long long int abs_second = abs((long long int)(offsetless_address -leaf_address));
          if (max < std::min((num_pages - abs_second), abs_second)){
            max = std::min((num_pages - abs_second), abs_second);
            *prev_adr = physical_adr + offset;
            *frame_to_evict = value;
            *max_leaf_address = leaf_address;
          }
        }
      }
      return max;
    }
    if(value != 0){
      max = DFS_for_leaf_helper(max, max_leaf_address,(vitrual_address << shift_size) + entry, shift_size,
                                cur_lvl + 1, max_lvl, num_pages, offsetless_address,
                                value * PAGE_SIZE, prev_adr, frame_to_evict);
    }
  }
  return max;

}

/***
 * this function find the leaf to evict according to the giving calculation in he instructions.
 * @param shift_size the size of the segments in the virtual address
 * @param tree_depth the depth of the tree
 * @param offsetless_address the virtual address without the offset
 * @param max_leaf_address a pointer to insert to the address of the chosen leaf address
 * @param prev_adr a pointer to insert to the previous frame
 * @param frame_to_evict a pointer to insert to the number of the frame to evict
 * @return the maximum value
 */
int DFS_for_leaf(int shift_size, int tree_depth, uint64_t offsetless_address, uint64_t* max_leaf_address,
                 int* prev_adr, uint64_t* frame_to_evict){
  int num_pages =1<<(VIRTUAL_ADDRESS_WIDTH- OFFSET_WIDTH);
  return DFS_for_leaf_helper(0,max_leaf_address, 0, shift_size, 0, tree_depth-1,
                             num_pages, offsetless_address, 0,prev_adr, frame_to_evict);
}

/***
 * Initialize the virtual memory.
 */
void VMinitialize(){
  for (int entry = 0; entry < PAGE_SIZE; entry++){
    PMwrite(entry,0);
  }
}

/**
 *
 * @param offsetless_adr the virtual address of the leaf without the offset which is not needed inorder to locate
 * the page
 * @param adder1 the frame number of the previous lvl
 * @param tree_lvls a list holding the list of segments for each lvl
 * @param lvl the current lvl in the table tree
 * @param max the next unused frame
 * @return the frame number of the previous lvl
 */
int load_new_frame(const uint64_t &offsetless_adr, int &adder1, const uint64_t *tree_lvls, int lvl, int max) {
  for (int offset = 0; offset < PAGE_SIZE; offset++) {
    PMwrite(max * PAGE_SIZE + offset,0);
  }
  PMwrite(adder1 * PAGE_SIZE + tree_lvls[lvl], max);
  if(lvl == TABLES_DEPTH - 1){
    PMrestore(max,offsetless_adr);
  }
  adder1 = max;
  return adder1;
}

/**
 * reuses the empty table found
 *
 * @param offsetless_adr the virtual address of the leaf without the offset which is not needed inorder to locate
 * the page
 * @param tree_lvls a list holding the list of segments for each lvl
 * @param prev_adder the physical address of the previous table
 * @param lvl the current lvl in the table tree
 * @param adder1 the frame number of the previous lvl
 * @param empty_table the number if the frame to be reused
 */
void replace_empty_table(const uint64_t &offsetless_adr, const uint64_t *tree_lvls, int prev_adder, int lvl,
                         int &adder1, int &empty_table) {
  PMwrite(prev_adder, 0);
  PMwrite(adder1 * PAGE_SIZE + tree_lvls[lvl], empty_table);
  adder1 = empty_table;
  if(lvl == TABLES_DEPTH - 1){
    PMrestore(empty_table,offsetless_adr);
  }
  empty_table = DEFAULT;
}

/***
 * evicts a frame from the ROM and stores it into the HARD DRIVE
 *
 * @param offsetless_adr the virtual address of the leaf without the offset which is not needed inorder to locate
 * the page
 * @param bit_seg the number of bits for each level in the table tree
 * @param tree_lvls a list holding the list of segments for each lvl
 * @param lvl the current lvl in the table tree
 * @param adder1 the frame number of the previous lvl
 * @param prev_adder the physical address of the previous table
 */
void evict_frame(const uint64_t &offsetless_adr, int bit_seg, const uint64_t *tree_lvls, int lvl, int &adder1,
                 int &prev_adder) {
  uint64_t max_leaf_address = DEFAULT;
  uint64_t frame_to_evict = DEFAULT;
  DFS_for_leaf(bit_seg,TABLES_DEPTH,offsetless_adr,&max_leaf_address, &prev_adder, &frame_to_evict);
  PMevict(frame_to_evict,max_leaf_address);
  PMwrite(prev_adder,0);
  for (int offset = 0; offset < PAGE_SIZE; offset++) {
    PMwrite(frame_to_evict * PAGE_SIZE + offset,0);
  }
  PMwrite(adder1 * PAGE_SIZE + tree_lvls[lvl], frame_to_evict);
  if(lvl == TABLES_DEPTH - 1){
    PMrestore(frame_to_evict,offsetless_adr);
  }
  adder1 = frame_to_evict;
}

/***
 * gets the virtual address and locates the corresponding page. if needed loads it into the ROM.
 * @param virtualAddress the virtual address of the leaf
 * @param final_offset the location inside the page to store or get data from
 * @param offsetless_adr the virtual address of the leaf without the offset which is not needed inorder to locate
 * the page
 * @param adder1 the frame number of the previous lvl
 */
void read_right_memory(const uint64_t &virtualAddress, uint64_t &final_offset, uint64_t &offsetless_adr, int &adder1) {
  final_offset= virtualAddress & ((int) (1 << OFFSET_WIDTH) - 1);
  offsetless_adr= virtualAddress >> OFFSET_WIDTH;
  adder1= 0;
  int bit_seg = CEIL((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) /
      (((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / (double)OFFSET_WIDTH)));
  uint64_t tree_lvls[TABLES_DEPTH];
  for (int tree_lvl = TABLES_DEPTH - 1, shift = 0; tree_lvl >= 0; tree_lvl--, shift++) {
    tree_lvls[tree_lvl] = (offsetless_adr >> (bit_seg*shift)) & ((int) (1<<bit_seg) - 1);
  }
  int adder2 = 0;
  int empty_table = DEFAULT;
  int prev_adder = DEFAULT;
  for (int lvl = 0 ; lvl < TABLES_DEPTH ; lvl++) {
    PMread(adder1 * PAGE_SIZE + tree_lvls[lvl], &adder2);
    if (adder2 == 0) {
      int max = DFS(TABLES_DEPTH, &empty_table, adder1, &prev_adder) + 1;
      if (empty_table != DEFAULT) {
        replace_empty_table(offsetless_adr, tree_lvls, prev_adder, lvl, adder1, empty_table);
        continue;
      }
      if (max < NUM_FRAMES) {
        adder1 = load_new_frame(offsetless_adr, adder1, tree_lvls, lvl, max);
        continue;
      }
      evict_frame(offsetless_adr, bit_seg, tree_lvls, lvl, adder1, prev_adder);
      continue;
    }
    adder1 = adder2;
  }
}



/***
 * Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value){
  if (0 >  virtualAddress || virtualAddress >= (1<<VIRTUAL_ADDRESS_WIDTH) ){
    return FAILURE;
  }
  uint64_t final_offset;
  uint64_t offsetless_adr;
  int adder1 = DEFAULT;
  read_right_memory(virtualAddress, final_offset, offsetless_adr, adder1);
  PMrestore(adder1,offsetless_adr);
  PMread(adder1 * PAGE_SIZE + final_offset, value);
  return SUCCESSES;
}


/***
 * Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value){
  if (0 >  virtualAddress || virtualAddress >= (1<<VIRTUAL_ADDRESS_WIDTH) ){
    return FAILURE;
  }
  uint64_t final_offset;
  uint64_t offsetless_adr;
  int adder1 = DEFAULT;
  read_right_memory(virtualAddress, final_offset, offsetless_adr, adder1);
  PMwrite(adder1 * PAGE_SIZE + final_offset, value);
  return SUCCESSES;
}
