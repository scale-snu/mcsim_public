/*************************************************************************/
/*                                                                       */
/*  C = A + B                                                            */
/*                                                                       */
/*  Command line options:                                                */
/*                                                                       */
/*  -pP : P = number of processors                                       */
/*  -nN : N = the length of a stream (word)                              */
/*  -rR : R = how many times to repeat C = A + B                         */
/*  -sS : S = page size (word)                                           */
/*  -cC : C = number of processors per cluster                           */
/*                                                                       */
/*  Default: ./STREAM -p1 -n1048576 -r4 -s512                            */
/*                                                                       */
/*************************************************************************/

#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

extern "C" {
extern void mcsim_skip_instrs_begin();
extern void mcsim_skip_instrs_end();
extern void mcsim_spinning_begin();
extern void mcsim_spinning_end();
int32_t log_2(uint64_t);
void * stream(void *);
}

const uint32_t max_processors = 1024;
const uint32_t num_words_per_cache_line = 8;

using namespace std;


class thread_args
{
 public:
  uint32_t proc_num;
  uint32_t num_processors;
  uint32_t num_repeats;
  uint32_t page_sz_in_word;
  uint64_t stream_length;
  double * init_pointer_A;
  double * init_pointer_B;
  double * init_pointer_C;
  double * value;
  pthread_barrier_t * barrier;
};


int main(int argc, char * argv[])
{
  mcsim_skip_instrs_begin();

  char c;

  uint32_t num_processors  = 1;
  uint32_t num_repeats     = 4;
  uint32_t page_sz_in_word = 512;
  uint64_t stream_length   = 1048576;

  while ((c = getopt(argc, (char * const *)argv, "p:r:n:s:h")) != -1)
  {
    switch(c)
    {
      case 'p':
        num_processors = atoi(optarg);
        if (num_processors < 1)
        {
          cerr << "P must be >= 1" << endl;
          exit(-1);
        }
        if (num_processors > max_processors)
        {
          cerr << "Maximum processors (max_processors) exceeded" << endl;
          exit(-1);
        }
        if (log_2(num_processors) == -1)
        {
          cerr << "The number of processors must be a power of 2" << endl;
          exit(-1);
        }
        break;
      case 'r':
        num_repeats = atoi(optarg);
        if (num_repeats < 1)
        {
          cerr << "R must be >= 1" << endl;
          exit(-1);
        }
        break;
      case 'n':
        stream_length = atoi(optarg);
        if (stream_length < 1)
        {
          cerr << "N must be >= 1" << endl;
          exit(-1);
        }
        break;
      case 's':
        page_sz_in_word = atoi(optarg);
        if (log_2(page_sz_in_word) == -1)
        {
          cerr << "The size of a page must be a power of 2" << endl;
          exit(-1);
        }
        break;
      case 'h':
        cout << "Usage: STREAM <options>" << endl << endl;
        cout << "   -pP : P = number of processors." << endl;
        cout << "   -nN : N = length of a stream." << endl;
        cout << "   -rR : R = number of repetitions." << endl;
        cout << "   -sS : S = page size in word." << endl;
        cout << "Default: STREAM -p" << num_processors << " -n" << stream_length << " -r" << num_repeats << " -s" << page_sz_in_word << endl;
        exit(0);
    }
  }

  pthread_barrier_t * barrier = new pthread_barrier_t;
  pthread_barrier_init(barrier, NULL, num_processors);
  pthread_t * threads = new pthread_t[num_processors];
  thread_args * th_args = new thread_args[num_processors];
  double * A = new double[stream_length + 8*page_sz_in_word*num_processors];
  double * B = new double[stream_length + 8*page_sz_in_word*num_processors];
  double * C = new double[stream_length + 8*page_sz_in_word*num_processors];

  A = (double *)((uint64_t)(A + 8*page_sz_in_word*num_processors - 1) / (8*page_sz_in_word*num_processors) * (8*page_sz_in_word*num_processors));
  B = (double *)((uint64_t)(B + 8*page_sz_in_word*num_processors - 1) / (8*page_sz_in_word*num_processors) * (8*page_sz_in_word*num_processors));
  C = (double *)((uint64_t)(C + 8*page_sz_in_word*num_processors - 1) / (8*page_sz_in_word*num_processors) * (8*page_sz_in_word*num_processors));

  if (stream_length%(page_sz_in_word*num_processors) != 0)
  {
    cerr << "The length of a stream must be a multiple of (page_sz_in_word * num_processors)" << endl;
    exit(-1);
  }

  for (uint32_t i = 0; i < num_processors; i++)
  {
    th_args[i].proc_num = i;
    th_args[i].num_processors= num_processors;
    th_args[i].num_repeats = num_repeats;
    th_args[i].page_sz_in_word = page_sz_in_word;
    th_args[i].stream_length = stream_length/num_processors;
    th_args[i].init_pointer_A = &(A[i*page_sz_in_word]);
    th_args[i].init_pointer_B = &(B[i*page_sz_in_word]);
    th_args[i].init_pointer_C = &(C[i*page_sz_in_word]);
    th_args[i].value          = new double;
    *(th_args[i].value)       = 0.0;
    th_args[i].barrier        = barrier;
  }

  mcsim_skip_instrs_end();
  double sum = 0.0;

  for (uint32_t i = 1; i < num_processors; i++)
  {
    pthread_create(&(threads[i]), NULL, stream, (void *)(&(th_args[i])));
  }
  stream((void *)(&(th_args[0])));
  sum += *(th_args[0].value);

  for (uint32_t i = 1; i < num_processors; i++)
  {
    pthread_join(threads[i], NULL);
    sum += *(th_args[i].value);
  }

  return 0;
}


void * stream(void * void_args)
{
  thread_args * args = (thread_args *) void_args;

  uint32_t num_repeats = args->num_repeats;
  uint32_t num_processors = args->num_processors;
  uint32_t page_sz_in_word = args->page_sz_in_word;
  uint64_t stream_length = args->stream_length;
  *(args->value) = 1.0;

  pthread_barrier_wait(args->barrier);
  for (uint32_t i = 0; i < num_repeats; i++)
  {
    double * init_pointer_A = args->init_pointer_A;
    double * init_pointer_B = args->init_pointer_B;
    double * init_pointer_C = args->init_pointer_C;

    for (uint64_t j = 0; j < stream_length; j+=page_sz_in_word)
    {
      for (uint64_t k = 0; k < page_sz_in_word; k+=8)
      {
        *(init_pointer_A) = *(init_pointer_B) + *(init_pointer_C);
        init_pointer_A+=8;
        init_pointer_B+=8;
        init_pointer_C+=8;
      }
      init_pointer_A+=page_sz_in_word*(num_processors - 1);
      init_pointer_B+=page_sz_in_word*(num_processors - 1);
      init_pointer_C+=page_sz_in_word*(num_processors - 1);
    }
  }

  return NULL;
}


int32_t log_2(uint64_t number)
{
  uint64_t cumulative = 1;
  int32_t out = 0;
  int done = 0;

  while ((cumulative < number) && (!done) && (out < 50)) {
    if (cumulative == number) {
      done = 1;
    } else {
      cumulative = cumulative * 2;
      out ++;
    }
  }

  if (cumulative == number) {
    return(out);
  } else {
    return(-1);
  }
}

