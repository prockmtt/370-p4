    /**
     * Project 1
     * Assembler code fragment for LC-2K
     */

    #define _POSIX_C_SOURCE 200809L

    #include <stdbool.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>

    //Every LC2K file will contain less than 1000 lines of assembly.
    #define MAXLINELENGTH 1000

    typedef struct labelPair {
        char* labelVal;
        // char[7] labelVal;
        int line;
    } labelPair;

    // labelPair init = {.labelVal = "", .line = -1};

    int readAndParse(FILE *, char *, char *, char *, char *, char *);
    static void checkForBlankLinesInCode(FILE *inFilePtr);
    static inline int isNumber(char *);
    static inline void printHexToFile(FILE *, int);
    int findLabel(labelPair*, char*, int);
    bool checkRegs(char*, char*, char*, char*);
    int calcTarget(int, int);

    int
    main(int argc, char **argv)
    {
        char *inFileString, *outFileString;
        FILE *inFilePtr, *outFilePtr;
        char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH],
                arg1[MAXLINELENGTH], arg2[MAXLINELENGTH];

        if (argc != 3) {
            printf("error: usage: %s <assembly-code-file> <machine-code-file>\n",
                argv[0]);
            exit(1);
        }

        inFileString = argv[1];
        outFileString = argv[2];

        inFilePtr = fopen(inFileString, "r");
        if (inFilePtr == NULL) {
            printf("error in opening %s\n", inFileString);
            exit(1);
        }

        // Check for blank lines in the middle of the code.
        checkForBlankLinesInCode(inFilePtr);

        outFilePtr = fopen(outFileString, "w");
        if (outFilePtr == NULL) {
            printf("error in opening %s\n", outFileString);
            exit(1);
        }

        // STEP 1: for each line, store pairs of [label, line]--
        // note: consider efficiency in data storage--
        // and check for errors in assembly code.

        labelPair arr[MAXLINELENGTH];
        // num of labels defined thus far in arr
        int eltsInArr = 0;
        int line = 0;
        while (readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)) {
            // while we haven't reached eof
            if (label[0] != '\0') {
                // line is preceded by label
                // labelIdx = idx of label in arr
                int labelIdx = findLabel(arr, label, eltsInArr);
                if (labelIdx > -1) {
                    // redefining a label s
                    printf("ERROR: label %s redefined at line %d\n", label, line);
                    exit(1);
                }
                else {
                    // printf("adding new label %s into arr\n", label);
                    // label not yet in arr
                    // maybe adjust so I'm doing strcpy() and the
                    // strings in labelPair are char[] not char*
                    char* copy = strdup(label);
                    labelPair newLabel = {copy, line};
                    arr[eltsInArr] = newLabel;
                    // increment num of labels in arr
                    eltsInArr += 1;
                }
            }
            if (strcmp(opcode, "add")     && strcmp(opcode, "lw")   && 
                strcmp(opcode, "sw")    && strcmp(opcode, "beq")  && 
                strcmp(opcode, "jalr")  && strcmp(opcode, "halt") && 
                strcmp(opcode, "noop")  && strcmp(opcode, "nor") && 
                strcmp(opcode, ".fill")) {
                // invalid opcode 
                printf("ERROR: opcode %s invalid\n", opcode);
                exit(1);
            }
            if (strcmp(opcode, "noop") && strcmp(opcode, "halt")) {
                // check register vals
                if (!checkRegs(opcode, arg0, arg1, arg2)) {
                    exit(1);
                }
            }
            if (!strcmp(opcode, "lw")   || !strcmp(opcode, "sw") || 
                !strcmp(opcode, "jalr") || !strcmp(opcode, "beq")) {
                // checking offsetfield--
                // check it's within valid range
                if (isNumber(arg2)) {
                    int argVal = atoi(arg2);
                    if (argVal + 32768 < 0 || argVal - 32767 > 0) {
                        printf("ERROR: offsetfield value %s invalid\n", arg2);
                        exit(1);
                }
                // if it's a label, put label in arr but Don't init line num
                else if (!isNumber(arg2) && arg2 != '\0' && 
                        findLabel(arr, arg2, eltsInArr) == -2) {
                    // printf("adding new label %s into arr\n", arg2);
                    char* copy = strdup(arg2);
                    labelPair newLabel = {copy, -1};
                    arr[eltsInArr] = newLabel;
                    // increment num of labels in arr
                    eltsInArr += 1;
                    }
                }
            }
            // increment current line num
            ++line;
        }

        // STEP 2: for each line, replace label with appropriate line # 
        // and translate into machine code, fprintf final line to output

        rewind(inFilePtr);
        int pc = 0;
        while (readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2)) {
            int regA = atoi(arg0);
            int regB = atoi(arg1);
            if (!strcmp(opcode, "noop")) {
                // 111
                int output = 0b111 << 22;
                printHexToFile(outFilePtr, output);
            }
            else if (!strcmp(opcode, "halt")) {
                // 110
                int output = 0b110 << 22;
                printHexToFile(outFilePtr, output);
            }
            else if (!strcmp(opcode, ".fill")) {
                int output;
                if (!isNumber(arg0)) {
                    // label 
                    int lineNum = findLabel(arr, arg0, eltsInArr);
                    if (lineNum < 0) {
                        printf("ERROR: undefined label used\n");
                        exit(1);
                    }
                    output = arr[lineNum].line;
                }
                else {
                    // just val
                    output = atoi(arg0);
                }
                printHexToFile(outFilePtr, output);
            }
            // everything from here on out will start with opcode regA regB;
            // jalr followed by 0s, add/nor followed by regDest, lw/sw/beq followed
            // by offsetField
            else if (!strcmp(opcode, "jalr")) {
                // 101
                int output = 0b101 << 22;
                output = output | (regA << 19);
                output = output | (regB << 16);
                printHexToFile(outFilePtr, output);
            }
            else if (!strcmp(opcode, "add")) {
                // 000
                int output = 0b000 << 22;
                output = output | (regA << 19);
                output = output | (regB << 16);
                int regDest = atoi(arg2);
                // 3 least sign. bits contain regDest
                output = output | (regDest);
                printHexToFile(outFilePtr, output);
            }
            else if (!strcmp(opcode, "nor")) {
                // 001
                int output = 0b001 << 22;
                output = output | (regA << 19);
                output = output | (regB << 16);
                int regDest = atoi(arg2);
                // 3 least sign. bits contain regDest
                output = output | (regDest);
                printHexToFile(outFilePtr, output);
            }
            // an unfortunate amt of code duplication below :-(
            else if (!strcmp(opcode, "beq")) {
                // 100
                int output = 0b100 << 22;
                output = output | (regA << 19);
                output = output | (regB << 16);
                int lineNum;
                int offsetVal;
                int lastSixteen;
                if (isNumber(arg2)) {
                    offsetVal = atoi(arg2);
                    int mask = ((1 << 16) - 1);
                    lastSixteen = offsetVal & mask;
                }
                else {
                    lineNum = findLabel(arr, arg2, eltsInArr);
                    if (lineNum < 0) {
                        printf("ERROR: undefined label used\n");
                        exit(1);
                    }
                    int target = arr[lineNum].line;
                    lastSixteen = calcTarget(target, pc);
                }
                // int lineNum = findLabel(arr, arg2, eltsInArr);
                // int target = arr[lineNum].line;
                // int lastSixteen;
                // if (lineNum == -2) {
                //     // not a label, just a value
                //     int offsetVal = atoi(arg2);
                //     int mask = ((1 << 16) - 1);
                //     lastSixteen = offsetVal & mask;
                // }
                // else {
                //     lastSixteen = calcTarget(target, pc);
                // }
                output = output | lastSixteen;
                printHexToFile(outFilePtr, output);
            }
            else if (!strcmp(opcode, "lw")) {
                // 010
                int output = 0b010 << 22;
                output = output | (regA << 19);
                output = output | (regB << 16);
                int lineNum;
                int offsetVal;
                if (isNumber(arg2)) {
                    offsetVal = atoi(arg2);
                }
                else {
                    lineNum = findLabel(arr, arg2, eltsInArr);
                    if (lineNum < 0) {
                        printf("ERROR: undefined label used\n");
                        exit(1);
                    }
                    offsetVal = arr[lineNum].line;
                }
                int mask = ((1 << 16) - 1);
                int lastSixteen = offsetVal & mask;
                output = output | lastSixteen;
                printHexToFile(outFilePtr, output);
            }
            else {
                // sw, 011
                int output = 0b011 << 22;
                output = output | (regA << 19);
                output = output | (regB << 16);
                int lineNum;
                int offsetVal;
                if (isNumber(arg2)) {
                    offsetVal = atoi(arg2);
                }
                else {
                    lineNum = findLabel(arr, arg2, eltsInArr);
                    if (lineNum < 0) {
                        printf("ERROR: undefined label used\n");
                        exit(1);
                    }
                    offsetVal = arr[lineNum].line;
                }
                int mask = ((1 << 16) - 1);
                int lastSixteen = offsetVal & mask;
                output = output | lastSixteen;
                printHexToFile(outFilePtr, output);
            }
            ++pc;
        }
        return(0);
    }

    // checks whether registers are valid (values of [0, 7] numeric only)
    bool checkRegs(char* opcode, char* arg0, char* arg1, char* arg2) {
        if (!strcmp(opcode, ".fill")) {
            return 1;
        }
        if (!(isNumber(arg0) && isNumber(arg1))) {
            printf("ERROR: one or both of %s, %s are non-numeric\n", arg0, arg1);
            return 0;
        }
        if (!strcmp(opcode, "add") || !strcmp(opcode, "nor")) {
            if (!(isNumber(arg2))) {
                printf("ERROR: third reg %s for opcode %s is non-numeric\n", arg2, opcode);
                return 0;
            }
            // check that reg3 between 0 and 7
            int reg3 = atoi(arg2);
            if (reg3 < 0 || reg3 > 7) {
                printf("ERROR: reg %s not between 0 and 7\n", arg2);
                return 0;
            }
        }
        // check that reg1 and reg2 between 0 and 7
        int reg0 = atoi(arg0);
        
        if (reg0 < 0 || reg0 > 7) {
            printf("ERROR: reg %s not between 0 and 7\n", arg0);
            return 0;
        }
        int reg1 = atoi(arg1);
        if (reg1 < 0 || reg1 > 7) {
            printf("ERROR: reg %s not between 0 and 7\n", arg1);
            return 0;
        }
        return 1;
    }

    // Returns index of label if it exists in arr, else returns -2
    int findLabel(labelPair* arr, char* labelIn, int eltsInArr) {
        for (int i = 0; i < eltsInArr; ++i) {
            if (!strcmp(arr[i].labelVal,labelIn)) {
                return i;
            }
        }
        return -2;
    }

    // uses offset value to calculate target address added to output
    int calcTarget(int val, int pc) {
        // target = pc + 1 + offset
        int offsetVal = val - pc - 1;
        // printf("target = %d pc = %d offset val = %d\n", val, pc, offsetVal);
        int mask = ((1 << 16) - 1);
        int lastSixteen = offsetVal & mask;
        return lastSixteen;
    }

    // Returns non-zero if the line contains only whitespace.
    static int lineIsBlank(char *line) {
        char whitespace[4] = {'\t', '\n', '\r', ' '};
        int nonempty_line = 0;
        for(int line_idx=0; line_idx < strlen(line); ++line_idx) {
            int line_char_is_whitespace = 0;
            for(int whitespace_idx = 0; whitespace_idx < 4; ++ whitespace_idx) {
                if(line[line_idx] == whitespace[whitespace_idx]) {
                    line_char_is_whitespace = 1;
                    break;
                }
            }
            if(!line_char_is_whitespace) {
                nonempty_line = 1;
                break;
            }
        }
        return !nonempty_line;
    }

    // Exits 2 if file contains an empty line anywhere other than at the end of the file.
    // Note calling this function rewinds inFilePtr.
    static void checkForBlankLinesInCode(FILE *inFilePtr) {
        char line[MAXLINELENGTH];
        int blank_line_encountered = 0;
        int address_of_blank_line = 0;
        rewind(inFilePtr);

        for(int address = 0; fgets(line, MAXLINELENGTH, inFilePtr) != NULL; ++address) {
            // Check for line too long
            if (strlen(line) >= MAXLINELENGTH-1) {
                printf("error: line too long\n");
                exit(1);
            }

            // Check for blank line.
            if(lineIsBlank(line)) {
                if(!blank_line_encountered) {
                    blank_line_encountered = 1;
                    address_of_blank_line = address;
                }
            } else {
                if(blank_line_encountered) {
                    printf("Invalid Assembly: Empty line at address %d\n", address_of_blank_line);
                    exit(2);
                }
            }
        }
        rewind(inFilePtr);
    }


    /*
    * NOTE: The code defined below is not to be modifed as it is implimented correctly.
    */

    /*
    * Read and parse a line of the assembly-language file.  Fields are returned
    * in label, opcode, arg0, arg1, arg2 (these strings must have memory already
    * allocated to them).
    *
    * Return values:
    *     0 if reached end of file
    *     1 if all went well
    *
    * exit(1) if line is too long.
    */
    int
    readAndParse(FILE *inFilePtr, char *label, char *opcode, char *arg0,
        char *arg1, char *arg2)
    {
        char line[MAXLINELENGTH];
        char *ptr = line;

        /* delete prior values */
        label[0] = opcode[0] = arg0[0] = arg1[0] = arg2[0] = '\0';

        /* read the line from the assembly-language file */
        if (fgets(line, MAXLINELENGTH, inFilePtr) == NULL) {
        /* reached end of file */
            return(0);
        }

        /* check for line too long */
        if (strlen(line) == MAXLINELENGTH-1) {
        printf("error: line too long\n");
        exit(1);
        }

        // Ignore blank lines at the end of the file.
        if(lineIsBlank(line)) {
            return 0;
        }

        /* is there a label? */
        ptr = line;
        if (sscanf(ptr, "%[^\t\n ]", label)) {
        /* successfully read label; advance pointer over the label */
            ptr += strlen(label);
        }

        /*
        * Parse the rest of the line.  Would be nice to have real regular
        * expressions, but scanf will suffice.
        */
        sscanf(ptr, "%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]",
            opcode, arg0, arg1, arg2);

        return(1);
    }

    static inline int
    isNumber(char *string)
    {
        int num;
        char c;
        return((sscanf(string, "%d%c",&num, &c)) == 1);
    }


    // Prints a machine code word in the proper hex format to the file
    static inline void 
    printHexToFile(FILE *outFilePtr, int word) {
        fprintf(outFilePtr, "0x%08X\n", word);
    }
