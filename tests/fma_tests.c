#include "fma_tests.h"

#define NB_ELEMENTS_TESTVECTOR 2
#define NB_TIMING_RUNS 1
#define READ_BUFFER_SIZE 1024

#define FLAG_PRECISION       1
#define FLAG_UNDERFLOW       2
#define FLAG_OVERFLOW        4
#define FLAG_DIVIDE_BY_ZERO  8
#define FLAG_INVALID        16
#define FLAG_ALL_SET (                      \
                      FLAG_PRECISION      | \
                      FLAG_UNDERFLOW      | \
                      FLAG_OVERFLOW       | \
                      FLAG_DIVIDE_BY_ZERO | \
                      FLAG_INVALID          \
		     )


#define ROUND_RD       0
#define ROUND_RN       1
#define ROUND_RA       2
#define ROUND_RZ       3
#define ROUND_RU       4

#define ROUND_DEC_RD       0
#define ROUND_DEC_RN       1
#define ROUND_DEC_RA       2
#define ROUND_DEC_RZ       3
#define ROUND_DEC_RU       4

#define PARSE_ERROR 0
#define EMPTY_LINE  1
#define PARSE_OKAY  2

/* An assembly macro to get the time counter */

/* time must be a pointer to an unsigned long long int */
#define READ_TIME_COUNTER(time)                              \
        __asm__ __volatile__(                                \
                "xorl %%eax,%%eax\n\t"                       \
                "cpuid\n\t"                                  \
                "rdtsc\n\t"                                  \
                "movl %%eax,(%0)\n\t"                        \
                "movl %%edx,4(%0)\n\t"                       \
                "xorl %%eax,%%eax\n\t"                       \
                "cpuid\n\t"                                  \
                : /* nothing */                              \
		: "S"((time))				     \
                : "eax", "ebx", "ecx", "edx", "memory")


void *mycalloc(size_t nmemb, size_t size) {
  void *ptr;

  ptr = calloc(nmemb, size);
  if (ptr == NULL) {
    fprintf(stderr,"Could not allocate any additional memory; exiting\n");
    exit(17);
  } 

  return ptr;
}

char *readTestVectorLine(FILE *fd, char* buf, size_t bufSize, char **rest, size_t *restSize) {
  char *line = NULL;
  size_t lineSize = 0;
  size_t lineUsed = 0;
  char *curr;
  char *temp;
  size_t elementsRead;
  int ferr;
  size_t i;
  
  if (*restSize > 0) {
    line = (char *) mycalloc(*restSize + 1, sizeof(char));
    lineSize = *restSize + 1;
    lineUsed = 0;
    curr = line; 
    for (i=0;i<*restSize;i++) {
      if ((*rest)[i] == '\n') {
	*rest = &((*rest)[i + 1]);
	*restSize -= (i + 1);
	return line;
      }
      *curr = (*rest)[i];
      curr++;
      lineUsed++;
    }
  }

  while (1) {
    clearerr(fd);
    elementsRead = fread(buf, sizeof(char), bufSize, fd);
    ferr = ferror(fd);
    if (elementsRead != 0) {
      if (line == NULL) {
	line = (char *) mycalloc(elementsRead + 1, sizeof(char));
	lineSize = elementsRead + 1;
	lineUsed = 0;
	curr = line; 
      }
      for (i=0;i<elementsRead;i++) {
	if (buf[i] == '\n') {
	  *rest = &(buf[i + 1]);
	  *restSize = elementsRead - (i + 1);
	  return line;
	}
	if (lineUsed > lineSize - 2) {
	  temp = (char *) mycalloc(lineSize * 2 + 1, sizeof(char));
	  lineSize = lineSize * 2 + 1;
	  memcpy(temp, line, lineUsed);
	  free(line);
	  line = temp;
	  curr = line + lineUsed;
	}
	*curr = buf[i];
	curr++;
	lineUsed++;
      }
    } 
    if (elementsRead < bufSize) {
      if (feof(fd)) {
	break;
      } 
      /* Here, we could not read enough characters but we haven't reached eof */
      if (ferr != 0) {
	fprintf(stderr, "Some I/O error occurred while reading in the test vector file; exiting.\n");
	exit(17);
      }
      /* Hugh, no error occurred but we could read enough? Let's try again! */
    }
  }
  
  return line;
}

int tryReadHexadecimal(uint64_t *res, char *str) {
  uint64_t acc, digit, temp;
  char *curr;
  
  if (*str == '\0') return 0; 

  acc = 0ull;
  for (curr=str;*curr!='\0';curr++) {
    if ((*curr >= '0') && (*curr <= '9')) {
      digit = (uint64_t) (*curr - '0');
    } else {
      if ((*curr >= 'a') && (*curr <= 'f')) {
	digit = (uint64_t) ((*curr - 'a') + 0xa);
      } else {
	if ((*curr >= 'A') && (*curr <= 'F')) {
	  digit = (uint64_t) ((*curr - 'A') + 0xa);
	} else {
	  return 0;
	}
      }
    }
    
    /* Integrate the digit into the accumulator
       stop when there is any overflow 
    */
    temp = acc << 4;
    if ((temp >> 4) == acc) {
      temp += digit;
      if (temp >= digit) {
	acc = temp;
      } else {
	return 0;
      }
    } else {
      return 0;
    }
  }

  *res = acc;
  return 1;
}

int tryReadDecimal(uint64_t *res, char *str) {
  uint64_t acc, digit, temp1, temp2;
  char *curr;

  if (*str == '\0') return 0; 
  
  acc = 0ull;
  for (curr=str;*curr!='\0';curr++) {
    if ((*curr >= '0') && (*curr <= '9')) {
      digit = (uint64_t) (*curr - '0');
    } else {
      return 0;
    }
    
    /* Integrate the digit into the accumulator
       stop when there is any overflow 

       acc = acc * 10 + digit

       acc = acc * 8 + acc * 2 + digit

    */
    temp1 = acc << 3;
    temp2 = acc << 1;
    if (((temp1 >> 3) == acc) && 
	((temp2 >> 1) == acc)) {
      temp2 += digit;
      if (temp2 >= digit) {
	temp1 += temp2;
	if (temp1 >= temp2) {
	  acc = temp1;
	} else {
	  return 0;
	}
      } else {
	return 0;
      }
    } else {
      return 0;
    }
  }

  *res = acc;
  return 1;
}

int tryReadDecimalSigned(int *res, char *str) {
  int negative;
  int acc;
  uint64_t abs, bound;
  int64_t temp;
  char *curr;

  if (str[0] == '-') {
    negative = 1;
    curr = str + 1;
  } else {
    negative = 0;
    curr = str;
  }

  if (!tryReadDecimal(&abs, curr)) {
    return 0;
  }

  if (negative) {
    temp = INT_MIN;
    temp = -temp;
    bound = temp;
    if (abs > bound) return 0;
    acc = -abs;
  } else {
    temp = INT_MAX;
    bound = temp;
    if (abs > bound) return 0;
    acc = abs;
  }

  *res = acc;
  return 1;
}

/* Returns non zero value if the flags are correctly defined.
   Sets res with the value of the flags to be raised.

   Returns a zero value if the flags are not correctly defined
   (raised and lowered at the same time or non defined character)
 */
int tryReadFlags(int *res, char *str) {
  int flagsToBeRaised, flagsToBeLowered;
  char *curr;

  if (*str == '\0') return 0;

  flagsToBeRaised = 0;
  flagsToBeLowered = 0;
  for (curr=str;*curr!='\0';curr++) {
    switch (*curr) {
    case 'p':
      flagsToBeLowered |= FLAG_PRECISION;
      break;
    case 'i':
      flagsToBeLowered |= FLAG_INVALID;
      break;
    case 'u':
      flagsToBeLowered |= FLAG_UNDERFLOW;
      break;
    case 'o':
      flagsToBeLowered |= FLAG_OVERFLOW;
      break;
    case 'z':
      flagsToBeLowered |= FLAG_DIVIDE_BY_ZERO;
      break;
    case 'P':
      flagsToBeRaised |= FLAG_PRECISION;
      break;
    case 'I':
      flagsToBeRaised |= FLAG_INVALID;
      break;
    case 'U':
      flagsToBeRaised |= FLAG_UNDERFLOW;
      break;
    case 'O':
      flagsToBeRaised |= FLAG_OVERFLOW;
      break;
    case 'Z':
      flagsToBeRaised |= FLAG_DIVIDE_BY_ZERO;
      break;
    default:
      return 0;
    }
  }

  flagsToBeLowered &= FLAG_ALL_SET;
  flagsToBeRaised &= FLAG_ALL_SET;
  if ((flagsToBeLowered & flagsToBeRaised) != 0) return 0;

  *res = flagsToBeRaised;
  return 1;
}

/* Returns non zero value if the rounding mode is correctly defined.
   Sets res with the value of the rounding mode.

   Returns a zero value if the rounding mode is correctly defined.
   (non defined expression)
 */
int tryReadRoundingMode(int *res, char *str) {
  char *curr;

  if (*str == '\0') return 0;

  for (curr=str;*curr!='\0';curr++) {
    switch (*curr) {
    case 'R':
      break;
    case 'U':
      *res = ROUND_RU;
      break;
    case 'D':
      *res = ROUND_RD;
      break;
    case 'Z':
      *res = ROUND_RZ;
      break;
    case 'N':
      *res = ROUND_RN;
      break;
    default:
      return 0;
    }
  }

  return 1;
}

int parseLine_binary64_binary64_binary64_decimal64(char *origline, __mixed_fma_testvector_binary64_binary64_binary64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;
  
  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;
  
 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_binary64_binary64_decimal64(FILE *fd, __mixed_fma_testvector_binary64_binary64_binary64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_binary64_binary64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_binary64_binary64_decimal64_binary64(char *origline, __mixed_fma_testvector_binary64_binary64_decimal64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_binary64_decimal64_binary64(FILE *fd, __mixed_fma_testvector_binary64_binary64_decimal64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_binary64_decimal64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_binary64_binary64_decimal64_decimal64(char *origline, __mixed_fma_testvector_binary64_binary64_decimal64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_binary64_decimal64_decimal64(FILE *fd, __mixed_fma_testvector_binary64_binary64_decimal64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_binary64_decimal64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_binary64_decimal64_binary64_binary64(char *origline, __mixed_fma_testvector_binary64_decimal64_binary64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_decimal64_binary64_binary64(FILE *fd, __mixed_fma_testvector_binary64_decimal64_binary64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_decimal64_binary64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_binary64_decimal64_binary64_decimal64(char *origline, __mixed_fma_testvector_binary64_decimal64_binary64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_decimal64_binary64_decimal64(FILE *fd, __mixed_fma_testvector_binary64_decimal64_binary64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_decimal64_binary64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_binary64_decimal64_decimal64_binary64(char *origline, __mixed_fma_testvector_binary64_decimal64_decimal64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_decimal64_decimal64_binary64(FILE *fd, __mixed_fma_testvector_binary64_decimal64_decimal64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_decimal64_decimal64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_binary64_decimal64_decimal64_decimal64(char *origline, __mixed_fma_testvector_binary64_decimal64_decimal64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeBinary(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_binary64_decimal64_decimal64_decimal64(FILE *fd, __mixed_fma_testvector_binary64_decimal64_decimal64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_binary64_decimal64_decimal64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_decimal64_binary64_binary64_binary64(char *origline, __mixed_fma_testvector_decimal64_binary64_binary64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_binary64_binary64_binary64(FILE *fd, __mixed_fma_testvector_decimal64_binary64_binary64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_binary64_binary64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_decimal64_binary64_binary64_decimal64(char *origline, __mixed_fma_testvector_decimal64_binary64_binary64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_binary64_binary64_decimal64(FILE *fd, __mixed_fma_testvector_decimal64_binary64_binary64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_binary64_binary64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_decimal64_binary64_decimal64_binary64(char *origline, __mixed_fma_testvector_decimal64_binary64_decimal64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_binary64_decimal64_binary64(FILE *fd, __mixed_fma_testvector_decimal64_binary64_decimal64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_binary64_decimal64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_decimal64_binary64_decimal64_decimal64(char *origline, __mixed_fma_testvector_decimal64_binary64_decimal64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeBinary(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_binary64_decimal64_decimal64(FILE *fd, __mixed_fma_testvector_decimal64_binary64_decimal64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_binary64_decimal64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}
int parseLine_decimal64_decimal64_binary64_binary64(char *origline, __mixed_fma_testvector_decimal64_decimal64_binary64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_decimal64_binary64_binary64(FILE *fd, __mixed_fma_testvector_decimal64_decimal64_binary64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_decimal64_binary64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}


int parseLine_decimal64_decimal64_binary64_decimal64(char *origline, __mixed_fma_testvector_decimal64_decimal64_binary64_decimal64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;
  
  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	currWord++;
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_binary64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_decimal64_t *) &uint64temp);
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  currWord += 3;
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeBinary(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeDecimal(negative, inttemp, uint64temp);
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
  
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_decimal64_binary64_decimal64(FILE *fd, __mixed_fma_testvector_decimal64_decimal64_binary64_decimal64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_decimal64_binary64_decimal64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }
    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

int parseLine_decimal64_decimal64_decimal64_binary64(char *origline, __mixed_fma_testvector_decimal64_decimal64_decimal64_binary64_t *testvector) {
  char *start, *curr;
  int maxWordCount, wordCount, parseOkay, currWord;
  char **words;
  char *line;
  uint64_t uint64temp;
  int inttemp;
  int negative;
  int nb;

  line = (char *) mycalloc(strlen(origline) + 1, sizeof(char));
  strcpy(line, origline);

  /* Trim of leading and trailing whitespace */
  for (start=line;*start!='\0';start++) {
    if ((*start != ' ') && 
	(*start != '\t')) break;
  }

  for (curr=start;*curr!='\0';curr++);
  for (;curr>=start;curr--) {
    if ((*curr != ' ') && 
	(*curr != '\t') && 
	(*curr != '\0')) break;
    *curr = '\0';
  }

  /* EZ return for empty lines */
  if (*start == '\0') {
    free(line);
    return EMPTY_LINE;
  }

  /* Check for comment lines */
  if ((*start == '/') && (*(start + 1) == '/')) {
    free(line);
    return EMPTY_LINE;
  }

  /* Cut the line into words delimited by whitespace */
  maxWordCount = strlen(start);
  words = (char **) mycalloc(maxWordCount, sizeof(char *));
  wordCount = 1;
  words[wordCount-1]=start;
  for (curr=start;*curr!='\0';curr++) {
    if ((*curr == ' ') || 
	(*curr == '\t')) {
      for (;*curr!='\0';curr++) {
	if ((*curr != ' ') && 
	    (*curr != '\t')) break;
	*curr = '\0';
      }
      if (*curr != '\0') {
	wordCount++;
	words[wordCount-1]=curr;
      }
    }
  }

  /* Analyze the different words */
  parseOkay = 1;
  currWord = 0;

  
  /* Read 4 numbers decimal64 or binary64 in two possible formats:

     - 0xdeadbeefcafebabe (in hexadecimal memory notation)

     - sign expo mantissa (with expo and mantissa in decimal notation)

  */
  for (nb=0;nb<4;nb++) {
    if ((wordCount >= 1 + currWord) && /* at least 1 word left to be read */
	(strlen(words[currWord]) == 2 + 16) && /* 0x => 2, 16 hexadecimal digits follow */
	(words[currWord][0] == '0') && 
	(words[currWord][1] == 'x')) {
      if (tryReadHexadecimal(&uint64temp, &(words[currWord][2]))) {
	if (nb==0) testvector->expectedResult = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==1) testvector->x = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==2) testvector->y = *((__mixed_fma_decimal64_t *) &uint64temp);
	if (nb==3) testvector->z = *((__mixed_fma_binary64_t *) &uint64temp);
	currWord++;
      } else {
	parseOkay = 0;
      }
    } else {
      if ((wordCount >= 3 + currWord) && /* at least 3 words left for sign, expo and mantissa */
	  (strlen(words[currWord]) == 1) && /* 1 character for the sign */
	  ((words[currWord][0] == '+') || (words[currWord][0] == '-'))) {
	negative = (words[currWord][0] == '-');
	if (tryReadDecimalSigned(&inttemp, words[currWord + 1]) &&
	    tryReadDecimal(&uint64temp, words[currWord + 2])) {
	  if (nb==0) testvector->expectedResult = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==1) testvector->x = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==2) testvector->y = composeDecimal(negative, inttemp, uint64temp);
	  if (nb==3) testvector->z = composeBinary(negative, inttemp, uint64temp);
	  currWord += 3;
	} else {
	  parseOkay = 0;
	}
      } else {
	parseOkay = 0;
      }
    }
    if (!parseOkay) goto getMeOuttaHere;
  }
 
  /* Now read two words puioz PUIOZ for the before and after flags */
  if (wordCount >= 2 + currWord) {
    if (tryReadFlags(&(testvector->beforeFlags), words[currWord]) &&
	tryReadFlags(&(testvector->expectedFlags), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

  /* Now read the binary and decimal rounding modes */
  if (wordCount >= 2 + currWord) {
    if (tryReadRoundingMode(&(testvector->binaryRoundingMode), words[currWord]) &&
	tryReadRoundingMode(&(testvector->decimalRoundingMode), words[currWord + 1])) {
      currWord += 2;
    } else {
      parseOkay = 0;
    }
  } else {
    parseOkay = 0;
  }
  if (!parseOkay) goto getMeOuttaHere;

 getMeOuttaHere:
  free(words);
  free(line);
  if (parseOkay) return PARSE_OKAY;
  return PARSE_ERROR;
}

int readTestVectorChunk_decimal64_decimal64_decimal64_binary64(FILE *fd, __mixed_fma_testvector_decimal64_decimal64_decimal64_binary64_t *testvector, int maxElements) {
  char *currLine;
  int i;
  char readBuffer[READ_BUFFER_SIZE];
  char *bufRest = NULL;
  size_t bufRestSize = 0;
  int parseRes;

  for (i=0;i<maxElements;) {
    /* Read a line */
    if ((currLine = readTestVectorLine(fd, readBuffer, READ_BUFFER_SIZE, &bufRest, &bufRestSize)) == NULL) {
      /* Could not read a line anymore */
      return i;
    }
   
    /* Parse it */
    if ((parseRes = parseLine_decimal64_decimal64_decimal64_binary64(currLine, &(testvector[i]))) != PARSE_OKAY) {
      /* Could not parse the line, skip it */
      if (parseRes != EMPTY_LINE) 
	fprintf(stderr, "Could not parse the test vector line \"%s\", skipping it.\n", currLine);
    } else {
      i++;
    }

    /* Free the line */
    free(currLine);
  }

  if (bufRestSize > 0) {
    if (fseek(fd, -((long) bufRestSize), SEEK_CUR) != 0) {
      fprintf(stderr, "Error during fseek: %s\n", strerror(errno));
    } 
  }   
  return maxElements;
}

/* Returns a string with upper case character if the flag is raised
   and lower case character if the flag is lowered.   
*/
void flagsToString(char *str, int flags) {
  if (flags & FLAG_PRECISION)      str[0] = 'P'; else str[0] = 'p';
  if (flags & FLAG_UNDERFLOW)      str[1] = 'U'; else str[1] = 'u';
  if (flags & FLAG_OVERFLOW)       str[2] = 'O'; else str[2] = 'o';
  if (flags & FLAG_DIVIDE_BY_ZERO) str[3] = 'Z'; else str[3] = 'z';
  if (flags & FLAG_INVALID)        str[4] = 'I'; else str[4] = 'i';
}

/* Set the flags */
void setFlags(int flags) {
  int excepts = 0;

  if (flags & FLAG_PRECISION)      excepts |= FE_INEXACT;
  if (flags & FLAG_UNDERFLOW)      excepts |= FE_UNDERFLOW;
  if (flags & FLAG_OVERFLOW)       excepts |= FE_OVERFLOW;
  if (flags & FLAG_DIVIDE_BY_ZERO) excepts |= FE_DIVBYZERO;
  if (flags & FLAG_INVALID)        excepts |= FE_INVALID;
 
  feclearexcept(FE_ALL_EXCEPT);
  feraiseexcept(excepts);
}

/* Get the current flags */
int getFlags() {
  int excepts, flags;

  excepts = fetestexcept(FE_ALL_EXCEPT);
  
  flags = 0;
  if (excepts & FE_INEXACT)   flags |= FLAG_PRECISION;
  if (excepts & FE_UNDERFLOW) flags |= FLAG_UNDERFLOW;
  if (excepts & FE_OVERFLOW)  flags |= FLAG_OVERFLOW;
  if (excepts & FE_DIVBYZERO) flags |= FLAG_DIVIDE_BY_ZERO;
  if (excepts & FE_INVALID)   flags |= FLAG_INVALID;
 
  return flags;
}

/* Set the string str to the rounding mode */
void roundingModesToString(char *str, int rm) {
  str[0] = 'R';
  if (rm == ROUND_RU)      str[1] = 'U';
  if (rm == ROUND_RD)      str[1] = 'D'; 
  if (rm == ROUND_RZ)      str[1] = 'Z'; 
  if (rm == ROUND_RN)      str[1] = 'N';
  if (rm == ROUND_RA)      str[1] = 'A'; 
}

/* Set the binary and decimal rounding modes */
void setRoundingModes(int rm_bin, int rm_dec) {
  int rm;

  /* Binary case */
  if (rm_bin == ROUND_RU)      rm = FE_UPWARD;
  if (rm_bin == ROUND_RD)      rm = FE_DOWNWARD;
  if (rm_bin == ROUND_RZ)      rm = FE_TOWARDZERO;
  if (rm_bin == ROUND_RN)      rm = FE_TONEAREST;

  fesetround(rm);
  
  /* Decimal case */
  if (rm_dec == ROUND_RU)      rm = ROUND_DEC_RU;
  if (rm_dec == ROUND_RD)      rm = ROUND_DEC_RD;
  if (rm_dec == ROUND_RZ)      rm = ROUND_DEC_RZ;
  if (rm_dec == ROUND_RN)      rm = ROUND_DEC_RN;
  if (rm_dec == ROUND_RA)      rm = ROUND_DEC_RA;

  __dfp_set_round(rm);
}

/* Get the binary and decimal rounding modes */
void getRoundingModes(int *rm_bin, int *rm_dec) {
  int rm;
  
  /* Binary case */
  rm = fegetround();
  if (rm == FE_UPWARD)        *rm_bin = ROUND_RU;
  if (rm == FE_DOWNWARD)      *rm_bin = ROUND_RD;
  if (rm == FE_TOWARDZERO)    *rm_bin = ROUND_RZ;
  if (rm == FE_TONEAREST)     *rm_bin = ROUND_RN;

  /* Decimal case */
  rm = __dfp_get_round();
  if (rm == ROUND_DEC_RU)     *rm_dec = ROUND_RU;
  if (rm == ROUND_DEC_RD)     *rm_dec = ROUND_RD;
  if (rm == ROUND_DEC_RZ)     *rm_dec = ROUND_RZ;
  if (rm == ROUND_DEC_RN)     *rm_dec = ROUND_RN;
  if (rm == ROUND_DEC_RA)     *rm_dec = ROUND_RA;
}

/* Compare flags1 and flags2.
   Returns a zero value if the same flags are raised in flags1 and flags2.
 */
int compareFlags(int flags1, int flags2) {
  return ((flags1 & FLAG_ALL_SET) == (flags2 & FLAG_ALL_SET));
}

/* Truncates the histogram table of the white noise that accumulate at the end
   and only keeps significand values such that they represent at least 5% of 
   the maximim value inside the table
*/
/* void truncate_hist(uint64_t *hist, uint32_t n){ */
/*   uint64_t max=0; */
/*   int i;   */
/* } */

void print_resultBBBD(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z, int bin_rm, int dec_rm) {
  char str_z[128] = { '\0' };
  char str_bin_rm[3] = { '\0' };
  char str_dec_rm[3] = { '\0' };
  decimal64_to_string(str_z,z);
  roundingModesToString(str_bin_rm,bin_rm);
  roundingModesToString(str_dec_rm,dec_rm);
  /* printf("Expecting %.20lf \t\t and getting %.20lf \t\t for the operation \t\t %lf \t * \t %lf \t + \t %s\t with binary RM %s\t and decimal RM %s\n",expectedResult, computedResult, x, y, str_z,str_bin_rm,str_dec_rm); */
  printf("%lf \t * \t %lf \t + \t %s\n", x, y, str_z);
}

/* Returns the number of tests that failed */
void __TEST_fma_binary64_binary64_binary64_decimal64(__mixed_fma_testvector_binary64_binary64_binary64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_binary64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    /* setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode); */
    testvector[i].computedResult = fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_binary64_decimal64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    
    /* if (testvector[i].expectedResult != testvector[i].computedResult){ */
    /*   fprintf(stderr,ANSI_COLOR_RED"ERROR IMPL / SOLLYA"ANSI_COLOR_RESET" in fma_binary64_binary64_binary64_decimal64\n"); */
    /*   print_resultBBBD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z,testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode); */
    /*   nb_fails->tfail_impl_VS_Sollya+=1; */
    /* } */

  if (ref == 1) {
      R_ref = ref_fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);

      /* if (testvector[i].expectedResult != R_ref){ */
      /* 	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR SOLLYA / GMP"ANSI_COLOR_RESET" in ref_binary64_binary64_binary64_decimal64\n"); */
      /* 	print_resultBBBD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z,testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode); */
      /* 	nb_fails->tfail_Sollya_VS_GMP+=1; */
      /* } */

      if (testvector[i].computedResult != R_ref){
	/* fprintf(stderr,ANSI_COLOR_RED"ERROR IMPL / SOLLYA"ANSI_COLOR_RESET" in fma_binary64_binary64_binary64_decimal64\n"); */
      /* print_resultBBBD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z,testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode); */
	/* fprintf(stderr,ANSI_COLOR_YELLOW"ERROR IMPL / GMP"ANSI_COLOR_RESET" in fma_binary64_binary64_binary64_decimal64\n"); */
	print_resultBBBD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z,testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultBBDB(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  char str_y[128] = { '\0' };
  decimal64_to_string(str_y,y);
  printf("Expecting %lf \t\t and getting %lf \t\t for the operation \t\t %lf \t * \t %s \t + \t %lf\n",expectedResult, computedResult, x, str_y, z);
  char str[128] = { '\0' };
  __mixed_fma_decimal64_t Da = 0.1D;
  __mixed_fma_binary64_t Ba = 0.1;
  decimal64_to_string(str,Da);
  printf("rnd(0.1) : \nbin %lf\ndec %s\n",Ba,str);
}

void __TEST_fma_binary64_binary64_decimal64_binary64(__mixed_fma_testvector_binary64_binary64_decimal64_binary64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
    int i;
  __ref_binary64_t R_ref;  
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_binary64_decimal64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_binary64_decimal64_binary64: result is not the same as predicted in file\n");
      print_resultBBDB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_binary64_binary64_decimal64_binary64: result is not the same as predicted in file\n");
	print_resultBBDB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_binary64_decimal64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultBBDB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultBBDD(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_decimal64_t z) {
  char str_y[128] = { '\0' };
  char str_z[128] = { '\0' };
  decimal64_to_string(str_y,y);
  decimal64_to_string(str_z,z);
  printf("Expecting %lf \t\t and getting %lf \t\t for the operation \t\t %lf \t * \t %s \t + \t %s\n",expectedResult, computedResult, x, str_y, str_z);
}

void __TEST_fma_binary64_binary64_decimal64_decimal64(__mixed_fma_testvector_binary64_binary64_decimal64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_binary64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_binary64_decimal64_decimal64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_binary64_decimal64_decimal64: result is not the same as predicted in file\n");
      print_resultBBDD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_binary64_binary64_decimal64_decimal64: result is not the same as predicted in file\n");
	print_resultBBDD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_binary64_decimal64_decimal64: result is not the same as predicted in reference implementation\n");
	print_resultBBDD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultBDBB(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_binary64_t z) {
  char str_x[128] = { '\0' };
  decimal64_to_string(str_x,x);
  printf("Expecting %lf \t\t and getting %lf \t\t for the operation \t\t %s \t * \t %lf \t + \t %lf\n",expectedResult, computedResult, str_x, y, z);
}

void __TEST_fma_binary64_decimal64_binary64_binary64(__mixed_fma_testvector_binary64_decimal64_binary64_binary64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_binary64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_decimal64_binary64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_binary64_binary64: result is not the same as predicted in file\n");
      print_resultBDBB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_binary64_decimal64_binary64_binary64: result is not the same as predicted in file\n");
	print_resultBDBB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_binary64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultBDBB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultBDBD(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  char str_x[128] = { '\0' };
  char str_z[128] = { '\0' };
  decimal64_to_string(str_x,x);
  decimal64_to_string(str_z,z);
  printf("Expecting %lf \t\t and getting %lf \t\t for the operation \t\t %s \t * \t %lf \t + \t %s\n",expectedResult, computedResult, str_x, y, str_z);
}

void __TEST_fma_binary64_decimal64_binary64_decimal64(__mixed_fma_testvector_binary64_decimal64_binary64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_binary64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_decimal64_binary64_decimal64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_binary64_decimal64: result is not the same as predicted in file\n");
      print_resultBDBD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_binary64_decimal64_binary64_decimal64: result is not the same as predicted in file\n");
	print_resultBDBD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_binary64_decimal64: result is not the same as predicted in reference implementation\n");
	print_resultBDBD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}


void print_resultBDDB(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_decimal64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  char str_x[128] = { '\0' };
  char str_y[128] = { '\0' };
  decimal64_to_string(str_x,x);
  decimal64_to_string(str_y,y);
  printf("Expecting %lf \t\t and getting %lf \t\t for the operation \t\t %s \t * \t %s \t + \t %lf\n",expectedResult, computedResult, str_x, str_y, z);
}

void __TEST_fma_binary64_decimal64_decimal64_binary64(__mixed_fma_testvector_binary64_decimal64_decimal64_binary64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_binary64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_binary64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_decimal64_decimal64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_decimal64_binary64: result is not the same as predicted in file\n");
      print_resultBDDB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_binary64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_binary64_decimal64_decimal64_binary64: result is not the same as predicted in file\n");
	print_resultBDDB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_decimal64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultBDDB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultBDDD(__mixed_fma_binary64_t expectedResult, __mixed_fma_binary64_t computedResult,
		      __mixed_fma_decimal64_t x, __mixed_fma_decimal64_t y, __mixed_fma_decimal64_t z) {
  char str_x[128] = { '\0' };
  char str_y[128] = { '\0' };
  char str_z[128] = { '\0' };
  decimal64_to_string(str_x,x);
  decimal64_to_string(str_y,y);
  decimal64_to_string(str_z,z);
  printf("Expecting %lf \t\t and getting %lf \t\t for the operation \t\t %s \t * \t %s \t + \t %s\n",expectedResult, computedResult, str_x, str_y, str_z);
}

void __TEST_fma_binary64_decimal64_decimal64_decimal64(__mixed_fma_testvector_binary64_decimal64_decimal64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
 int i;
 __ref_binary64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_binary64_decimal64_decimal64_decimal64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_decimal64_decimal64: result is not the same as predicted in file\n");
      print_resultBDDD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_binary64_decimal64_decimal64_decimal64: result is not the same as predicted in file\n");
	print_resultBDDD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_binary64_decimal64_decimal64_decimal64: result is not the same as predicted in reference implementation\n");
	print_resultBDDD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultDBBB(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_binary64_t y, __mixed_fma_binary64_t z) {
  
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %lf \t * \t %lf \t + \t %lf\n",str_er, str_cr, x, y, z);
}

void __TEST_fma_decimal64_binary64_binary64_binary64(__mixed_fma_testvector_decimal64_binary64_binary64_binary64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_binary64_binary64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_binary64_binary64: result is not the same as predicted in file\n");
      print_resultDBBB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_binary64_binary64_binary64: result is not the same as predicted in file\n");
	print_resultDBBB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_binary64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultDBBB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultDBBD(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  char str_z[128] = { '\0' };
  decimal64_to_string(str_z,z);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %lf \t * \t %lf \t + \t %s\n",str_er, str_cr, x, y, str_z);
}

void __TEST_fma_decimal64_binary64_binary64_decimal64(__mixed_fma_testvector_decimal64_binary64_binary64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_binary64_binary64_decimal64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_binary64_decimal64: result is not the same as predicted in file\n");
      print_resultDBBD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_binary64_binary64_decimal64: result is not the same as predicted in file\n");
	print_resultDBBD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_binary64_decimal64: result is not the same as predicted in reference implementation\n");
	print_resultDBBD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultDBDB(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  char str_y[128] = { '\0' };
  decimal64_to_string(str_y,y);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %lf \t * \t %s \t + \t %lf\n",str_er, str_cr, x, str_y, z);
}

void __TEST_fma_decimal64_binary64_decimal64_binary64(__mixed_fma_testvector_decimal64_binary64_decimal64_binary64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_binary64_decimal64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_decimal64_binary64: result is not the same as predicted in file\n");
      print_resultDBDB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_binary64_decimal64_binary64: result is not the same as predicted in file\n");
	print_resultDBDB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_decimal64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultDBDB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultDBDD(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult,
		      __mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_decimal64_t z) {
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  char str_y[128] = { '\0' };
  char str_z[128] = { '\0' };
  decimal64_to_string(str_y,y);
  decimal64_to_string(str_z,z);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %lf \t * \t %s \t + \t %s\n",str_er, str_cr, x, str_y, str_z);
}


void __TEST_fma_decimal64_binary64_decimal64_decimal64(__mixed_fma_testvector_decimal64_binary64_decimal64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_binary64_decimal64_decimal64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_decimal64_decimal64: result is not the same as predicted in file\n");
      print_resultDBDD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_binary64_decimal64_decimal64: result is not the same as predicted in file\n");
	print_resultDBDD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_binary64_decimal64_decimal64: result is not the same as predicted in reference implementation\n");
	print_resultDBDD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultDDBB(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult,
		      __mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_binary64_t z) {
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  char str_x[128] = { '\0' };
  decimal64_to_string(str_x,x);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %s \t * \t %lf \t + \t %lf\n",str_er, str_cr, str_x, y, z);
}

void __TEST_fma_decimal64_decimal64_binary64_binary64(__mixed_fma_testvector_decimal64_decimal64_binary64_binary64_t *testvector,failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_decimal64_binary64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_decimal64_binary64_binary64: result is not the same as predicted in file\n");
      print_resultDDBB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_decimal64_binary64_binary64: result is not the same as predicted in file\n");
	print_resultDDBB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_decimal64_binary64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultDDBB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}


void print_resultDDBD(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult,
		       __mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  char str_x[128] = { '\0' };
  char str_z[128] = { '\0' };
  decimal64_to_string(str_x,x);
  decimal64_to_string(str_z,z);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %s \t * \t %lf \t + \t %s\n",str_er, str_cr, str_x, y, str_z);
}

void __TEST_fma_decimal64_decimal64_binary64_decimal64(__mixed_fma_testvector_decimal64_decimal64_binary64_decimal64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref) {
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_decimal64_binary64_decimal64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_decimal64_binary64_decimal64: result is not the same as predicted in file\n");
      print_resultDDBD(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_decimal64_binary64_decimal64: result is not the same as predicted in file\n");
	print_resultDDBD(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_decimal64_binary64_decimal64: result is not the same as predicted in reference implementation\n");
	print_resultDDBD(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void print_resultDDDB(__mixed_fma_decimal64_t expectedResult, __mixed_fma_decimal64_t computedResult, 
		     __mixed_fma_decimal64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  char str_er[128] = { '\0' };
  char str_cr[128] = { '\0' };
  char str_x[128] = { '\0' };
  char str_y[128] = { '\0' };
  decimal64_to_string(str_er,expectedResult);
  decimal64_to_string(str_cr,computedResult);
  decimal64_to_string(str_x,x);
  decimal64_to_string(str_y,y);
  printf("Expecting %s \t\t and getting %s \t\t for the operation \t\t %s \t * \t %s \t + \t %lf\n",str_er,str_cr, str_x, str_y,z);
}

void __TEST_fma_decimal64_decimal64_decimal64_binary64(__mixed_fma_testvector_decimal64_decimal64_decimal64_binary64_t *testvector, failedtests_t *nb_fails, int nb_tests, int ref){
  int i;
  __ref_decimal64_t R_ref;
  char computedFlagsString[] = "PUOZI";
  char expectedFlagsString[] = "PUOZI";
  
  for(i=0 ; i<nb_tests ; i++) {
    setFlags(testvector[i].beforeFlags);
    setRoundingModes(testvector[i].binaryRoundingMode,testvector[i].decimalRoundingMode);
    testvector[i].computedResult = fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
    testvector[i].computedFlags = getFlags();
    if (!compareFlags(testvector[i].expectedFlags, testvector[i].computedFlags)) {
      flagsToString(computedFlagsString, testvector[i].computedFlags);
      flagsToString(expectedFlagsString, testvector[i].expectedFlags);
      /* fprintf(stderr,"FLAG ERROR in fma_decimal64_decimal64_decimal64_binary64: expected %s, computed %s\n", expectedFlagsString, computedFlagsString); */
      nb_fails->tfail_flags+=1;
    }
    if (testvector[i].expectedResult != testvector[i].computedResult){
      fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_decimal64_decimal64_binary64: result is not the same as predicted in file\n");
      print_resultDDDB(testvector[i].expectedResult, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
      nb_fails->tfail_impl_VS_Sollya+=1;
    }
    if (ref == 1) {
      R_ref = ref_fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      if (testvector[i].expectedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_MAGENTA"ERROR REFERENCE"ANSI_COLOR_RESET" in ref_decimal64_decimal64_decimal64_binary64: result is not the same as predicted in file\n");
	print_resultDDDB(testvector[i].expectedResult, R_ref, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_Sollya_VS_GMP+=1;
      }
      if (testvector[i].computedResult != R_ref){
	fprintf(stderr,ANSI_COLOR_RED"ERROR"ANSI_COLOR_RESET" in fma_decimal64_decimal64_decimal64_binary64: result is not the same as predicted in reference implementation\n");
	print_resultDDDB(R_ref, testvector[i].computedResult, testvector[i].x, testvector[i].y,testvector[i].z);
	nb_fails->tfail_impl_VS_GMP+=1;
      }
    }
  }
}

void __TIME_fma_binary64_binary64_binary64_decimal64(__mixed_fma_testvector_binary64_binary64_binary64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref, __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 
  
  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
	
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }
    
    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
	
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
	
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}


void __TIME_fma_binary64_binary64_decimal64_binary64(__mixed_fma_testvector_binary64_binary64_decimal64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref, __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 
  
  if (ref == 1){
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}
  
void __TIME_fma_binary64_binary64_decimal64_decimal64(__mixed_fma_testvector_binary64_binary64_decimal64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_binary64_decimal64_binary64_binary64( __mixed_fma_testvector_binary64_decimal64_binary64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_binary64_decimal64_binary64_decimal64(__mixed_fma_testvector_binary64_decimal64_binary64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_binary64_decimal64_decimal64_binary64(__mixed_fma_testvector_binary64_decimal64_decimal64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_binary64_decimal64_decimal64_decimal64(__mixed_fma_testvector_binary64_decimal64_decimal64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_binary64_decimal64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_decimal64_binary64_binary64_binary64(__mixed_fma_testvector_decimal64_binary64_binary64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
	
	if (hist!=NULL) {
	  ti = (__mixed_fma_uint64_t) (timing / tscale);
	  if (ti>TMAX) {
	    hist[TMAX-1]++;
	  } else {
	    hist[ti]++;
	  }
	}
	
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }
    }
  }
}

void __TIME_fma_decimal64_binary64_binary64_decimal64(__mixed_fma_testvector_decimal64_binary64_binary64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_decimal64_binary64_decimal64_binary64(__mixed_fma_testvector_decimal64_binary64_decimal64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_decimal64_binary64_decimal64_decimal64(__mixed_fma_testvector_decimal64_binary64_decimal64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_binary64_decimal64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}

void __TIME_fma_decimal64_decimal64_binary64_binary64(__mixed_fma_testvector_decimal64_decimal64_binary64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_decimal64_binary64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}
void __TIME_fma_decimal64_decimal64_binary64_decimal64(__mixed_fma_testvector_decimal64_decimal64_binary64_decimal64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_decimal64_binary64_decimal64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}
void __TIME_fma_decimal64_decimal64_decimal64_binary64(__mixed_fma_testvector_decimal64_decimal64_decimal64_binary64_t *testvector, int nb_tests, int nb_timing_runs, timeanalytics_t *t, int ref,  __mixed_fma_uint64_t *hist, __mixed_fma_uint64_t TMAX, double tscale){
  int i, j;
  __mixed_fma_binary64_t fCT, timing;
  unsigned long long int before, after;
  __mixed_fma_uint64_t ti;
  
  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_binary64_t) (after - before);
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  fCT = 0.0;
/* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      empty_fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = (__mixed_fma_uint128_t) (after - before);
      } else {
	timing = (__mixed_fma_uint128_t) 0;
      }
      
      if (timing > t->tmax_empty){
	t->tmax_empty = timing;
      }

      fCT += timing;
    }
  }

  /* Get the average function call time */
  fCT /= ((__mixed_fma_binary64_t) (nb_tests * nb_timing_runs)); 

  if (ref == 1) {
    /* Run the timing nb_timing_runs times to head the cache */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      }
    }

    /* Get the maximum timing to run the reference function */
    for (j=0;j<nb_timing_runs;j++) {
      for (i=0;i<nb_tests;i++) {
	READ_TIME_COUNTER(&before);
	ref_fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
	READ_TIME_COUNTER(&after);
      
	if (after > before) {
	  timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
	} else {
	  timing = (__mixed_fma_binary64_t) 0.0;
	}
      
	if (timing > t->tmax_ref){
	  t->tmax_ref = timing;
	}
      }
    }
  }

  /* Run the timing nb_timing_runs times to head the cache */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
    }
  }

  /* Get the timing needed to call a function */
  for (j=0;j<nb_timing_runs;j++) {
    for (i=0;i<nb_tests;i++) {
      READ_TIME_COUNTER(&before);
      fma_decimal64_decimal64_decimal64_binary64(testvector[i].x, testvector[i].y, testvector[i].z);
      READ_TIME_COUNTER(&after);
      
      if (after > before) {
	timing = ((__mixed_fma_binary64_t) (after - before)) - fCT;
      } else {
	timing = (__mixed_fma_binary64_t) 0.0;
      }
      
      if (timing > t->tmax){
	t->tmax = timing;
      }

      if (hist!=NULL) {
	ti = (__mixed_fma_uint64_t) (timing / tscale);
	if (ti>TMAX) {
	  hist[TMAX-1]++;
	} else {
	  hist[ti]++;
	}
      }
    }
  }
}


int main(int argc, char **argv) {
  FILE *fd_in;

  FILE *fd_out;
  __mixed_fma_testvector_binary64_binary64_binary64_decimal64_t testvectorBBBD[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_binary64_binary64_decimal64_binary64_t testvectorBBDB[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_binary64_binary64_decimal64_decimal64_t testvectorBBDD[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_binary64_decimal64_binary64_binary64_t testvectorBDBB[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_binary64_decimal64_binary64_decimal64_t testvectorBDBD[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_binary64_decimal64_decimal64_binary64_t testvectorBDDB[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_binary64_decimal64_decimal64_decimal64_t testvectorBDDD[NB_ELEMENTS_TESTVECTOR];

  __mixed_fma_testvector_decimal64_binary64_binary64_binary64_t testvectorDBBB[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_decimal64_binary64_binary64_decimal64_t testvectorDBBD[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_decimal64_binary64_decimal64_binary64_t testvectorDBDB[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_decimal64_binary64_decimal64_decimal64_t testvectorDBDD[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_decimal64_decimal64_binary64_binary64_t testvectorDDBB[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_decimal64_decimal64_binary64_decimal64_t testvectorDDBD[NB_ELEMENTS_TESTVECTOR];
  __mixed_fma_testvector_decimal64_decimal64_decimal64_binary64_t testvectorDDDB[NB_ELEMENTS_TESTVECTOR];
  
  int nb_tests, ref, k;
  timeanalytics_t t;
  failedtests_t nb_tests_fail;
  __mixed_fma_uint64_t TMAX, nb_tests_total=0;
  __mixed_fma_uint64_t *hist;
  double tscale=1.0;

  memset(&t, 0, sizeof(t));

  t.tmax = 0.0;
  t.tmax_empty = 0.0;
  t.tmax_ref = 0.0;
  nb_tests_fail.tfail_impl_VS_GMP = 0;
  nb_tests_fail.tfail_impl_VS_Sollya = 0;
  nb_tests_fail.tfail_Sollya_VS_GMP = 0;
  nb_tests_fail.tfail_flags = 0;

  if (argc < 5) {
    fprintf(stderr, "Not enough arguments given.\nUsave:\n%s <function under test> <withreferencefunction=0 or 1> <testvectorfile> <histrogramresultfile>\n", argv[0]);
    fprintf(stderr, "Possible entries for the function under test:\n\tBBBD, BBDB, BBDD, BDBB, BDBD, BDDB, BDDD\n\tDBBB, DBBD, DBDB, DBDD, DDBB, DDBD, DDDB\n");
    exit(1);
  }

  if (strcmp(argv[2],"0")==1) {
    ref = 1;
  } else if (strcmp(argv[2],"0")==0) {
    ref = 0;
  } else {
    fprintf(stderr, "Wrong argument: %s is invalid.\nWrite:\n\t0 if you do not want to test against the reference implementation\n\t1 if you want to test against the reference implementation\n",argv[2]);
    exit(1);
  }

  if ((fd_in = fopen(argv[3],"r")) == NULL) {
    fprintf(stderr, "Could not open the test vector file \"%s\": %s\n", argv[3], strerror(errno));
    exit(1);
  }

  if ((fd_out = fopen(argv[4],"w")) == NULL) {
    fprintf(stderr, "Could not open the test vector file \"%s\": %s\n", argv[4], strerror(errno));
    exit(1);
  }
  
  if (strcmp(argv[1],"BBBD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_binary64_binary64_decimal64(fd_in, testvectorBBBD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_binary64_binary64_decimal64(testvectorBBBD,&nb_tests_fail,nb_tests,ref);
      /* printf("TIMNG TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n"); */
      /* /\* Max Timings *\/ */
      /* __TIME_fma_binary64_binary64_binary64_decimal64(testvectorBBBD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0); */
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_binary64_binary64_decimal64(fd_in, testvectorBBBD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_binary64_binary64_decimal64(testvectorBBBD,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */
    if (ref) {
      printf(ANSI_COLOR_CYAN"\nTOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL (IMPL/SOLLYA): %ld\n"ANSI_COLOR_RESET ANSI_COLOR_YELLOW"FAIL (IMPL/GMP): %ld\n"ANSI_COLOR_RESET ANSI_COLOR_MAGENTA"FAIL (SOLLYA/GMP): %ld\n"ANSI_COLOR_RESET "FLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail_impl_VS_Sollya, nb_tests_fail.tfail_impl_VS_GMP, nb_tests_fail.tfail_Sollya_VS_GMP ,nb_tests_fail.tfail_flags);
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
    
  } else if (strcmp(argv[1],"BBDB")==0) {
    /* printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_decimal64_binary64"ANSI_COLOR_RESET"\n"); */
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_binary64_decimal64_binary64(fd_in, testvectorBBDB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_binary64_decimal64_binary64(testvectorBBDB,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_binary64_binary64_decimal64_binary64(testvectorBBDB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_binary64_decimal64_binary64(fd_in, testvectorBBDB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_binary64_decimal64_binary64(testvectorBBDB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */    
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }

  } else if (strcmp(argv[1],"BBDD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_decimal64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_binary64_decimal64_decimal64(fd_in, testvectorBBDD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_binary64_decimal64_decimal64(testvectorBBDD,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_binary64_binary64_decimal64_decimal64(testvectorBBDD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_binary64_decimal64_decimal64(fd_in, testvectorBBDD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_binary64_decimal64_decimal64(testvectorBBDD,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_decimal64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_binary64_decimal64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"BDBB")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_binary64_binary64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_binary64_binary64(fd_in, testvectorBDBB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_decimal64_binary64_binary64(testvectorBDBB,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_binary64_decimal64_binary64_binary64(testvectorBDBB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_binary64_binary64(fd_in, testvectorBDBB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_decimal64_binary64_binary64(testvectorBDBB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_binary64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_binary64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"BDBD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_binary64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_binary64_decimal64(fd_in, testvectorBDBD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_decimal64_binary64_decimal64(testvectorBDBD,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_binary64_decimal64_binary64_decimal64(testvectorBDBD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_binary64_decimal64(fd_in, testvectorBDBD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_decimal64_binary64_decimal64(testvectorBDBD,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"BDDB")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_decimal64_binary64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_decimal64_binary64(fd_in, testvectorBDDB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_decimal64_decimal64_binary64(testvectorBDDB,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_binary64_decimal64_decimal64_binary64(testvectorBDDB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_decimal64_binary64(fd_in, testvectorBDDB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_decimal64_decimal64_binary64(testvectorBDDB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"BDDD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_decimal64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_decimal64_decimal64(fd_in, testvectorBDDD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_binary64_decimal64_decimal64_decimal64(testvectorBDDD,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_binary64_decimal64_decimal64_decimal64(testvectorBDDD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_binary64_decimal64_decimal64_decimal64(fd_in, testvectorBDDD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_binary64_decimal64_decimal64_decimal64(testvectorBDDD,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);

    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_decimal64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_binary64_decimal64_decimal64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"DBBB")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_binary64_binary64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_binary64_binary64(fd_in, testvectorDBBB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_binary64_binary64_binary64(testvectorDBBB,&nb_tests_fail,nb_tests, ref);
      /* Max timings */
      __TIME_fma_decimal64_binary64_binary64_binary64(testvectorDBBB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_binary64_binary64(fd_in, testvectorDBBB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_decimal64_binary64_binary64_binary64(testvectorDBBB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);
    
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_binary64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_binary64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"DBBD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_binary64_decimal64(fd_in, testvectorDBBD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_binary64_binary64_decimal64(testvectorDBBD,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_decimal64_binary64_binary64_decimal64(testvectorDBBD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_binary64_decimal64(fd_in, testvectorDBBD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_decimal64_binary64_binary64_decimal64(testvectorDBBD,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);
    
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"DBDB")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_decimal64_binary64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_decimal64_binary64(fd_in, testvectorDBDB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_binary64_decimal64_binary64(testvectorDBDB,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_decimal64_binary64_decimal64_binary64(testvectorDBDB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_decimal64_binary64(fd_in, testvectorDBDB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_decimal64_binary64_decimal64_binary64(testvectorDBDB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);
    
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"DBDD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_decimal64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_decimal64_decimal64(fd_in, testvectorDBDD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_binary64_decimal64_decimal64(testvectorDBDD,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_decimal64_binary64_decimal64_decimal64(testvectorDBDD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_binary64_decimal64_decimal64(fd_in, testvectorDBDD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_decimal64_binary64_decimal64_decimal64(testvectorDBDD,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);
    
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_decimal64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_binary64_decimal64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"DDBB")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_binary64_binary64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_decimal64_binary64_binary64(fd_in, testvectorDDBB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_decimal64_binary64_binary64(testvectorDDBB,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_decimal64_decimal64_binary64_binary64(testvectorDDBB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_decimal64_binary64_binary64(fd_in, testvectorDDBB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_decimal64_decimal64_binary64_binary64(testvectorDDBB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);
     
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_binary64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_binary64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else if (strcmp(argv[1],"DDBD")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_binary64_decimal64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_decimal64_binary64_decimal64(fd_in, testvectorDDBD, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_decimal64_binary64_decimal64(testvectorDDBD,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_decimal64_decimal64_binary64_decimal64(testvectorDDBD,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Put the offset back at the beginning of the file */
    fseek(fd_in,0,SEEK_SET);
    TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
    if (TMAX > 2000000) {
      tscale = ((double) TMAX) / ((double) 2000000);
      TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
    }
    hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
    for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_decimal64_binary64_decimal64(fd_in, testvectorDDBD, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      __TIME_fma_decimal64_decimal64_binary64_binary64(testvectorDDBB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    for (k=0;k<TMAX;k++) {
      if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
    }
    free(hist);
    /* Close the file and open it again for the second part of timing tests */
    fclose(fd_in);
    fd_in = fopen(argv[3],"r");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      
      /* Histrogram Timings */
      TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
      if (TMAX > 5000) {
	tscale = ((double) TMAX) / ((double) 5000);
	TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
      }
      hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
      for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
      
      for (k=0;k<TMAX;k++) if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
      free(hist);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_binary64_decimal64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
    
  } else if (strcmp(argv[1],"DDDB")==0) {
    printf("TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_decimal64_binary64"ANSI_COLOR_RESET"\n");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_decimal64_decimal64_binary64(fd_in, testvectorDDDB, NB_ELEMENTS_TESTVECTOR);
      /* Tests */
      __TEST_fma_decimal64_decimal64_decimal64_binary64(testvectorDDDB,&nb_tests_fail,nb_tests,ref);
      /* Max timings */
      __TIME_fma_decimal64_decimal64_decimal64_binary64(testvectorDDDB,nb_tests,NB_TIMING_RUNS,&t,ref,NULL,0,1.0);
      nb_tests_total+=nb_tests;
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    /* Close the file and open it again for the second part of timing tests */
    fclose(fd_in);
    fd_in = fopen(argv[3],"r");
    while(1) {
      /* Read a chunk of the file and set the test vector */
      nb_tests = readTestVectorChunk_decimal64_decimal64_decimal64_binary64(fd_in, testvectorDDDB, NB_ELEMENTS_TESTVECTOR);
      /* Histrogram Timings */
      TMAX = ((__mixed_fma_uint64_t) (t.tmax * 100 + 100));
      if (TMAX > 5000) {
	tscale = ((double) TMAX) / ((double) 5000);
	TMAX = (__mixed_fma_uint64_t) (((double) TMAX) / tscale);
      }
      hist = calloc(TMAX, sizeof(__mixed_fma_uint64_t));
      for (k=0;k<TMAX;k++) hist[k]=(__mixed_fma_uint64_t)0;
      __TIME_fma_decimal64_decimal64_decimal64_binary64(testvectorDDDB,nb_tests,NB_TIMING_RUNS,&t,ref,hist,TMAX,tscale);
      for (k=0;k<TMAX;k++) if (hist[k]!=0) fprintf(fd_out,"%d %ld\n",k,hist[k]);
      free(hist);
      if (nb_tests < NB_ELEMENTS_TESTVECTOR) break;
    }
    
    /* Print test recap */        
    if (ref) {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\n"ANSI_COLOR_MAGENTA"REF FAIL: %ld"ANSI_COLOR_RESET"\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags,nb_tests_fail.tfail_ref); */
    } else {
      /* printf(ANSI_COLOR_CYAN"TOTAL: %ld\n"ANSI_COLOR_RESET ANSI_COLOR_RED"FAIL: %ld"ANSI_COLOR_RESET"\nFLAGS FAIL: %ld\nEND TESTS for the function "ANSI_COLOR_BLUE"fma_decimal64_decimal64_decimal64_binary64"ANSI_COLOR_RESET"\n\n", nb_tests_total, nb_tests_fail.tfail,nb_tests_fail.tfail_flags); */
    }
  } else {
    fprintf(stderr, "Wrong argument, \"%s\" is not valid function under test\n", argv[1]);
    exit(1);
  }

  fclose(fd_in);
  fclose(fd_out);  
}
