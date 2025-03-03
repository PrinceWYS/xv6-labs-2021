struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  uint lastuse;
  struct buf *next;
  uchar data[BSIZE];

  int trash;  // indicate a buf which contains invalid dev and blockno
};

