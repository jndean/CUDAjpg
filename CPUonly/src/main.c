#include<stdio.h>
#include<stdlib.h>
#include<time.h>

#include<format.h>
#include<decodeScanCPU.h>
#include<pixelTransformCPU.h>


JPG* openJPG(const char *filename){
  JPG* jpg = newJPG(filename);
  if (NULL == jpg){
    printf("Failed to read %s\n", filename);
    return NULL;
  }

  // Main format block parsing loop //
  while(!jpg->error){
    if (jpg->pos > jpg->end - 2) {
      jpg->error = SYNTAX_ERROR;
      break;
    }
    if (jpg->pos[0] != 0xFF) {
      jpg->error = SYNTAX_ERROR;
      break;
    }
    
    jpg->pos += 2;
    switch(jpg->pos[-1]) {
    case 0xC0: decodeSOF(jpg); break;
    case 0xC4: decodeDHT(jpg); break;
    case 0xDB: decodeDQT(jpg); break;
    case 0xDD: decodeDRI(jpg); break;
    case 0xDA: decodeScanCPU(jpg); break;
    case 0xFE: skipBlock(jpg); break;
    case 0xD9: break;
    default:
      if((jpg->pos[-1] & 0xF0) == 0xE0) skipBlock(jpg);
      else jpg->error = SYNTAX_ERROR;
    }

    // Finished //
    if (jpg->pos[-1] == 0xD9) {
      upsampleAndColourTransform(jpg);
      break;
    }
  }

  if(jpg->error){
    fprintf(stderr, "Decode failed with error code %d\n", jpg->error);
    delJPG(jpg);
    return NULL;
  }

  return jpg;
}



int main(int argc, char** argv){
  
  if(argc < 2) {
    fprintf(stderr, "USAGE: %s filename.jpg ...\n", argv[0]);
    return EXIT_FAILURE;
  }

  int i, n = 100;
  double total_time = 0;
  for (i=0; i<n; i++){
    int filename_id = 1 + (i % (argc - 1));
    clock_t start = clock();
    JPG* jpg = openJPG(argv[filename_id]);
    total_time += (clock() - start);
    if (!jpg)
      printf("Failed to open jpg %s\n", argv[1]);
    delJPG(jpg);
  }

  printf("%0.3lfms per image\n", 1000.0 * total_time / (n * CLOCKS_PER_SEC));
  
  //char* filename = (jpg->num_channels == 1) ? "outfile.pgm" : "outfile.ppm";
  //writeJPG(jpg, filename);
  

  return EXIT_SUCCESS;
}
