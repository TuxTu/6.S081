struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  int num;     // use for debug
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  // uint tstamp; // the time stamp for LRU  
  // LRU cache list: in the reverse order of hash bucket (head->tail, node->prev) 
  struct buf *lruprev;
  struct buf *lrunext;
  // use in hash bucket
  struct buf *bucprev; 
  struct buf *bucnext; 
  uchar data[BSIZE];
};

