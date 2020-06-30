#include "stdint.h"
#include "string.h"
#include "photon.h"

#define N_VAL   16  // bytes in hash output
#define C_VAL   16  // internal state capacity in bytes
#define R_VAL   2   // internal state bitrate and message block size in bytes
#define T_VAL   (R_VAL+C_VAL) // total number of bytes in internal state
#define D_VAL   6   // # of cells in rows and columns in internal permutation
#define S_VAL   4   // bitsize of each cell
#define G_CON   0x3 // GF XOR constant used during multiplication


#define VAL_1X(N)       (N)
#define VAL_2X(N)       VAL_1X((N)), VAL_1X((N))
#define VAL_4X(N)       VAL_2X((N)), VAL_2X((N))
#define VAL_8X(N)       VAL_4X((N)), VAL_4X((N))
#define VAL_16X(N)      VAL_8X((N)), VAL_8X((N))
#define VAL_32X(N)      VAL_16X((N)), VAL_16X((N))
#define VAL_64X(N)      VAL_32X((N)), VAL_32X((N))
#define VAL_128X(N)     VAL_64X((N)), VAL_64X((N))
#define VAL_256X(N)     VAL_128X((N)), VAL_128X((N))

#define SWAP(A,B)   if(1){\
    (A) ^= (B);\
    (B) ^= (A);\
    (A) ^= (B);\
}


static const uint8_t sbox[16]={
  0xC,0x5,0x6,0xB,
  0x9,0x0,0xA,0xD,
  0x3,0xE,0xF,0x8,
  0x4,0x7,0x1,0x2
};

static const uint8_t A144[D_VAL*D_VAL]={
  0x1,0x2,0x8,0x5,0x8,0x2,
  0x2,0x5,0x1,0x2,0x6,0xC,
  0xC,0x9,0xF,0x8,0x8,0xD,
  0xD,0x5,0xB,0x3,0xA,0x1,
  0x1,0xF,0xD,0xE,0xB,0x8,
  0x8,0x2,0x3,0x3,0x2,0x8
};

// Would be better to straight hard code and make const
// Space also wasted as a x b and b x a are equal
// Will be filled in as hashing is performed to speed up mixColumnsSerial
// Products of At_val x state_val are at (16*At_val + state_val)
// Replacement values will not exceed 0x0F
//    -> Anything greater hasn't been calculated
static uint8_t MMUL[256]={ VAL_256X(0xF3) };

static const uint8_t IC[D_VAL]={0,1,3,7,6,4};
static const uint8_t RC[12]={
  1,3,7,14,
  13,11,6,12,
  9,2,5,10
};

static void messageFold(const uint8_t *message, uint8_t *cells);
static void internalPermutation(uint8_t *cells);
static void addConstants(uint8_t *cells, const int32_t round);
static void subCells(uint8_t *cells);
static void shiftRows(uint8_t *cells);
static void mixColumnsSerial(uint8_t *cells);
static void linearMix(uint8_t *cells, uint8_t col);
static uint8_t mul(uint8_t a, uint8_t b);

// Accepts a message of size bytes
// Returns hashed value of size N_VAL bytes
// Assumes *hash can hold N_VAL bytes

// message - Message to be hashed
// message_bytes - Number of bytes in input to be hashed
// hash - Output container to hold N_VAL hashed bytes
void photon128(uint8_t *message, const int32_t message_bytes, uint8_t *hash){
    uint8_t cells[T_VAL];
    int i;

    // Set IV values
    memset((uint8_t *) cells,0,T_VAL-3);
    cells[15]=0x20;
    cells[16]=0x10;
    cells[17]=0x10;

    // Fold in the messages
    for(i=0;i<message_bytes/R_VAL;i++){
        messageFold(&message[2*i],&cells[0]);
        internalPermutation(&cells[0]);
    }

    // Odd number of message bytes encountered
    if(message_bytes%2){
        cells[0] ^= message[i*2];
        cells[1] ^= 0x80; //Padding
    }
    else{ // Message is a multiple of r, perform padding
        cells[0] ^= 0x80;
        cells[1] ^= 0x00; // Padding
    }

    // Last time to absorb
    internalPermutation(&cells[0]);

    // Now time to squeeze
    // Set all N_VAL bytes of hash buffer to zero then extract the first bytes
    memset(hash,0,N_VAL);
    hash[0] |= cells[0];
    hash[1] |= cells[1];

    // Get the rest of the bytes
    for(i=1;i<N_VAL/2;i++){
        internalPermutation(&cells[0]);
        hash[2*i] |= cells[0];
        hash[(2*i)+1] |= cells[1];
    }

    // *hash should now contain a N_VAL byte hash
}

static void messageFold(const uint8_t *message, uint8_t *cells){
    cells[0] ^= message[0];
    cells[1] ^= message[1];
}

static void internalPermutation(uint8_t *cells){
    int round;
    for(round=0;round<12;round++){
        addConstants(cells,round);
        subCells(cells);
        shiftRows(cells);
        mixColumnsSerial(cells);
    }

}

static void addConstants(uint8_t *cells, const int32_t round){
    int i;
    for(i=0;i<D_VAL;i++){
        cells[i*T_VAL/D_VAL] ^= (RC[round] ^ IC[i]) << 4;
    }
}


static void subCells(uint8_t *cells){
    int i;
    for(i=0;i<T_VAL;i++){
        uint16_t tmp=cells[i];
        cells[i]=0;
        cells[i] |= sbox[tmp & 0xF];        // lower nibble
        cells[i] |= sbox[tmp >> 4] << 4;    // upper nibble
    }
}


static void shiftRows(uint8_t *cells){
    uint32_t tmp1;
    int i;
    for(i=1;i<D_VAL;i++){
        tmp1=0;
        tmp1|=(cells[i*3]<<16);
        tmp1|=(cells[(i*3)+1]<<8);
        tmp1|=(cells[(i*3)+2]);
        tmp1 = (tmp1 << (4*i)) | (tmp1 >> 4*(D_VAL-i));
        cells[i*3]=(tmp1 & 0xFF0000) >> 16;
        cells[(i*3)+1] = (tmp1 & 0xFF00) >> 8;
        cells[(i*3)+2] = (tmp1 & 0xFF);
    }
}


static void mixColumnsSerial(uint8_t *cells){
    int i=0;
    for(i=0;i<D_VAL;i++){
        linearMix(cells,i);
    }
}

static void linearMix(uint8_t *cells, uint8_t col){
    uint8_t tmp_cells[D_VAL];
    int i,j;

    // XOR Muls of S[col,i] and A[i,col] to get
    //   each value of S[col,0] to S[col,D_VAL-1]
    for(i=0;i<D_VAL;i++){
        tmp_cells[i]=0;
        for(j=0;j<D_VAL;j++){
            uint8_t nib = cells[(col+(D_VAL*j))/2];
            if(col%2 == 0){nib = (nib & 0xF0) >> 4;}
            else{nib = (nib & 0x0F);}
            tmp_cells[i] ^= (mul(A144[(D_VAL*i)+j],nib));
        }
    }

    for(i=0;i<D_VAL;i++){
        if(col%2 == 0){
            cells[(col+(D_VAL*i))/2] &= 0x0F;
            cells[(col+(D_VAL*i))/2] |= (tmp_cells[i] << 4);
        }
        else{
            cells[(col+(D_VAL*i))/2] &= 0xF0;
            cells[(col+(D_VAL*i))/2] |= tmp_cells[i];
        }
    }
}

// Accepts two nibbles (4 bits) and returns 4 bit value used in mixing stage
static uint8_t mul(uint8_t a, uint8_t b){
    uint8_t ret = MMUL[(16*a)+b];

    // Sanity check to enforce nibble inputs
    a &= 0x0F;
    b &= 0x0F;

    // a x b not previously calculated
    if(ret > 0x0F){
        uint8_t b_orig; // Used to update MMUL correctly
        uint8_t loc;
        uint8_t odd = 0;


        if(a<b){SWAP(a,b);} // Want a to be larger to reduce computation

        if(b==0){return b;} // Multiply a by 0 is 0
        if(b==1){return a;} // Multiply a by 1 is a

        b_orig=b;
        ret=a;              // Running product

        // Multiply by even number b=2^n
        // Multiply by odd number b=(2^n)+1
        // Adds are actually just XORs
        while(b>1){
            loc=(ret*16)+b;
            if(MMUL[loc]<0x10){
                ret=MMUL[loc];
                b=0;
            }
            else{
                // Account for odd value as (a x 3) is (a x (2 + 1))
                if(b&1){
                    odd ^= ret;
                }

                loc=ret; // loc holds the current value to be multiplied by 2
                // (a x 2) not found
                if(MMUL[32 + loc] > 0xF){
                    ret = ret << 1;
                    // XOR with constant if shifted results is larger than 4 bits
                    if(ret&0x10){ret^=G_CON;}

                    ret &= 0xF; // Only care about lower nibble
                    MMUL[32 + loc] = ret;
                    MMUL[16*loc + 2] = ret;
                }
                else{
                    ret = MMUL[32 + loc];
                }
                b = b >> 1;
            }
        }
        ret ^= odd; // One last "add" if multiplying by odd number at any point
        ret &= 0xF; // Only care about lower nibble

        // Update map so won't have to recompute a x b
        MMUL[(a*16)+b_orig]=ret;
        MMUL[(b_orig*16)+a]=ret;
    }
    return ret;
}

