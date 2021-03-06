#include<stdio.h>
#include<string.h>

#include<format.h>
#include<decodeScanCPU.h>
#include<pixelTransformCPU.h>


// This only shows the bits, but doesn't move past them //
int showBits(JPG* jpg, int num_bits){
  unsigned char newbyte;
  if(!num_bits) return 0;

  while (jpg->num_bufbits < num_bits){
    if(jpg->pos >= jpg->end){
      jpg->bufbits = (jpg->bufbits << 8) | 0xFF;
      jpg->num_bufbits += 8;
      continue;
    }
    newbyte = *jpg->pos++;
    jpg->bufbits = (jpg->bufbits << 8) | newbyte;
    jpg->num_bufbits += 8;
    if (newbyte != 0xFF)
      continue;
	
    if(jpg->pos >= jpg->end)
      goto overflow_error;
    
    // Handle byte stuffing //
    unsigned char follow_byte = *jpg->pos++;
    switch (follow_byte){
    case 0x00:
    case 0xFF:
    case 0xD9:
      break;
    default:
      if ((follow_byte & 0xF8) != 0xD0){
	printf("The follow_byte case that doesn't have to be 0x00?\n");
	goto overflow_error;
      } else {
	printf("The acceptable non-zero followbyte case?\n");
	jpg->bufbits = (jpg->bufbits << 8) | newbyte;
	jpg->num_bufbits += 8;
      }
    }
  }
  return (jpg->bufbits >> (jpg->num_bufbits - num_bits)) & ((1 << num_bits) - 1);

 overflow_error:
  printf("Huffman decode overflow?\n");
  jpg->error = SYNTAX_ERROR;
  return 0;
}


// Show the bits AND move past them //
int getBits(JPG* jpg, int num_bits){
  int res = showBits(jpg, num_bits);
  jpg->num_bufbits -= num_bits;
  return res;
}


int getVLC(JPG* jpg, DhtVlc* vlc_table, unsigned char* code){
  int symbol = showBits(jpg, 16);
  DhtVlc vlc = vlc_table[symbol];
  if(!vlc.num_bits){
    jpg->error = SYNTAX_ERROR;
    return 0;
  }
  jpg->num_bufbits -= vlc.num_bits;  
  if(code) *code = vlc.tuple;
  unsigned char num_bits = vlc.tuple & 0x0F;
  if (!num_bits) return 0;
  int value = getBits(jpg, num_bits);
  if (value < (1 << (num_bits - 1)))
    value += ((-1) << num_bits) + 1;
  return value;  
}


void decodeBlock(JPG* jpg, ColourChannel* channel, unsigned char* out){
  unsigned char code = 0;
  int value, coef = 0;
  int* block = jpg->block_space;
  memset(block, 0, 64 * sizeof(int));

  // Read DC value //
  channel->dc_cumulative_val += getVLC(jpg, &jpg->vlc_tables[channel->dc_id][0], NULL);
  block[0] = (channel->dc_cumulative_val) * jpg->dq_tables[channel->dq_id][0];
  // Read  AC values //
  do {
    value = getVLC(jpg, &jpg->vlc_tables[channel->ac_id][0], &code);
    if (!code) break; // EOB marker //
    if (!(code & 0x0F) && (code != 0xF0)) THROW(SYNTAX_ERROR);
    coef += (code >> 4) + 1;
    if (coef > 63) THROW(SYNTAX_ERROR);
    block[(int)deZigZag[coef]] = value * jpg->dq_tables[channel->dq_id][coef];
  } while(coef < 63);

  // Invert the DCT //
  for (coef = 0;  coef < 64;  coef += 8)
    iDCT_row(&block[coef]);
  for (coef = 0;  coef < 8;  ++coef)
  iDCT_col(&block[coef], &out[coef], channel->stride);
}


void decodeScanCPU(JPG* jpg){
  unsigned char *pos = jpg->pos;
  unsigned int header_len = read16(pos);
  if (pos + header_len >= jpg->end) THROW(SYNTAX_ERROR);
  pos += 2;

  if (header_len < (4 + 2 * jpg->num_channels)) THROW(SYNTAX_ERROR);
  if (*(pos++) != jpg->num_channels) THROW(UNSUPPORTED_ERROR);
  int i; ColourChannel *channel;
  for(i = 0, channel=jpg->channels; i<jpg->num_channels; i++, channel++, pos+=2){
    if (pos[0] != channel->id) THROW(SYNTAX_ERROR);
    if (pos[1] & 0xEE) THROW(SYNTAX_ERROR);
    channel->dc_id = pos[1] >> 4;
    channel->ac_id = (pos[1] & 1) | 2;
  }
  if (pos[0] || (pos[1] != 63) || pos[2]) THROW(UNSUPPORTED_ERROR);
  pos = jpg->pos = jpg->pos + header_len;


  int restart_interval = jpg->restart_interval;
  int restart_count = restart_interval;
  int next_restart_index = 0;
  
  // Loop over all blocks
  for (int block_y = 0; block_y < jpg->num_blocks_y; block_y++){
    for (int block_x = 0; block_x < jpg->num_blocks_x; block_x++){

      // Loop over all channels //
      for (i = 0, channel = jpg->channels; i < jpg->num_channels; i++, channel++){

	// Loop over samples in block //
	for (int sample_y = 0; sample_y < channel->samples_y; ++sample_y){
	  for (int sample_x = 0; sample_x < channel->samples_x; ++sample_x){
	    
	    int out_pos = ((block_y * channel->samples_y + sample_y) * channel->stride
			   + block_x * channel->samples_x + sample_x) << 3;
	    decodeBlock(jpg, channel, &channel->pixels[out_pos]);
	    if (jpg->error) return;
	  }
	}
      }

      if (restart_interval && !(--restart_count)){
	printf("Doing a restart\n");
	// Byte align //
	jpg->num_bufbits &= 0xF8;
	i = getBits(jpg, 16);
	if (((i & 0xFFF8) != 0xFFD0) || ((i & 7) != next_restart_index))
	  THROW(SYNTAX_ERROR);
	next_restart_index = (next_restart_index + 1) & 7;
	restart_count = restart_interval;
	for (i = 0; i < 3; i++)
	  jpg->channels[i].dc_cumulative_val = 0;
      }
    }
  }
 
}
